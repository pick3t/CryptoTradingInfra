#ifndef CRYPTO_TRADING_INFRA_RING_BUFFER
#define CRYPTO_TRADING_INFRA_RING_BUFFER

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "math.hpp"

template <typename T>
class alignas(std::hardware_destructive_interference_size) PaddedAtomic
{
    std::atomic<T> value;
    char padding[std::hardware_destructive_interference_size - sizeof(value)];
public:
    std::atomic<T>& Inner()
    {
        return value;
    }

    const std::atomic<T>& Inner() const
    {
        return value;
    }

    operator std::atomic<T>&()
    {
        return value;
    }

    operator const std::atomic<T>&() const
    {
        return value;
    }
};

constexpr size_t DEFAULT_CAPACITY = 1024;
template <typename T, size_t Capacity = DEFAULT_CAPACITY>
class ConcurrentRingBuffer
{
    PaddedAtomic<size_t> head;
    PaddedAtomic<size_t> tail;

    struct Node {
        std::atomic<size_t> seq;
        T data;
    };

    static constexpr auto CAP = NextPowerOf2<Capacity>();

    using Container = std::vector<Node>;
    Container buffer;

    template <typename F>
    bool AcquireAndSet(F&& setNodeData)
    {
        size_t pos = tail.Inner().load(std::memory_order_relaxed);

        while (true) {
            // (CAP - 1) acts as a mask to deal with wrap-around
            auto& node = buffer[pos & (CAP - 1)];
            size_t seq = node.seq.load(std::memory_order_acquire);
            intptr_t dif = seq - pos;

            if (dif == 0) {
                if (tail.Inner().compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    setNodeData(node.data);
                    node.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }

            if (dif < 0) {
                return false; // buffer is full
            }

            pos = tail.Inner().load(std::memory_order_relaxed);
        }
    }

public:
    ConcurrentRingBuffer() : head {}, tail {}, buffer { CAP }
    {
        for (size_t i = 0; i < CAP; ++i) {
            buffer[i].seq.store(i, std::memory_order_relaxed);
        }
    }

    ConcurrentRingBuffer(const ConcurrentRingBuffer&) = delete;
    ConcurrentRingBuffer& operator=(const ConcurrentRingBuffer&) = delete;

    ConcurrentRingBuffer(ConcurrentRingBuffer&&) = delete;
    ConcurrentRingBuffer& operator=(ConcurrentRingBuffer&&) = delete;

    template <typename U = T>
    bool Push(U&& item)
    {
        return AcquireAndSet([&](T& data) {
            data = std::forward<U>(item);
        });
    }

    template <typename... Args>
    bool Emplace(Args&&... args)
    {
        return AcquireAndSet([&](T& data) {
            new (&data) T(std::forward<Args>(args)...);
        });
    }

    bool Pop(T& item)
    {
        size_t pos = head.Inner().load(std::memory_order_relaxed);

        while (true) {
            auto& node = buffer[pos & (CAP - 1)];
            size_t seq = node.seq.load(std::memory_order_acquire);
            intptr_t dif = seq - (pos + 1);

            if (dif == 0) {
                if (head.Inner().compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    item = std::move(node.data);
                    node.seq.store(pos + CAP, std::memory_order_release);
                    return true;
                }
            }

            if (dif < 0) {
                return false; // buffer is empty
            }

            pos = head.Inner().load(std::memory_order_relaxed);
        }
    }

    // Empty and Full only offer a quick snapshot of the current ring buffer and the result may immediately expire
    // once called
    bool Empty() const
    {
        size_t h = head.Inner().load(std::memory_order_acquire);
        size_t t = tail.Inner().load(std::memory_order_acquire);
        return h == t;
    }

    bool Full() const
    {
        size_t t = tail.Inner().load(std::memory_order_acquire);
        size_t h = head.Inner().load(std::memory_order_acquire);
        // We assume CAP fits in size_t and wrapping is well-defined
        return (t - h) >= CAP;
    }
};

#endif
