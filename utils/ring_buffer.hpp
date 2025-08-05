#ifndef CRYPTO_TRADING_INFRA_RING_BUFFER
#define CRYPTO_TRADING_INFRA_RING_BUFFER

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <new>
#include <vector>

#include "math.hpp"

/* sourced from examples in cpp reference, size of cache line is typically 64 bytes on most modern systems*/
#ifdef __cpp_lib_hardware_interference_size
    using std::hardware_constructive_interference_size;
    using std::hardware_destructive_interference_size;
#else
    constexpr std::size_t hardware_constructive_interference_size = 64;
    constexpr std::size_t hardware_destructive_interference_size = 64;
#endif

#define CACHE_LINE_ALIGNED alignas(hardware_destructive_interference_size)

constexpr size_t DEFAULT_CAPACITY = 1024;
template <typename T, size_t Capacity = DEFAULT_CAPACITY>
class ConcurrentRingBuffer
{
    CACHE_LINE_ALIGNED std::atomic<uint64_t> head;
    CACHE_LINE_ALIGNED std::atomic<uint64_t> tail;

    struct Node {
        std::atomic<uint64_t> seq;
        T data;
    };

    static constexpr auto CAP = NextPowerOf2<Capacity>();

    using Container = std::vector<Node>;
    Container buffer;

    template <typename F>
    bool AcquireAndSet(F&& setNodeData)
    {
        auto pos = tail.load(std::memory_order_relaxed);

        while (true) {
            // (CAP - 1) acts as a mask to deal with wrap-around
            auto& node = buffer[pos & (CAP - 1)];
            auto seq = node.seq.load(std::memory_order_acquire);
            int64_t dif = seq - pos;

            if (dif == 0) {
                if (tail.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    setNodeData(node.data);
                    node.seq.store(pos + 1, std::memory_order_release);
                    return true;
                }
            }

            if (dif < 0) {
                return false; // buffer is full
            }

            pos = tail.load(std::memory_order_relaxed);
        }
    }

public:
    ConcurrentRingBuffer() : head {}, tail {}, buffer { CAP }
    {
        for (auto i = 0; i < CAP; ++i) {
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
        auto pos = head.load(std::memory_order_relaxed);

        while (true) {
            auto& node = buffer[pos & (CAP - 1)];
            auto seq = node.seq.load(std::memory_order_acquire);
            int64_t dif = seq - (pos + 1);

            if (dif == 0) {
                if (head.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) {
                    item = std::move(node.data);
                    node.seq.store(pos + CAP, std::memory_order_release);
                    return true;
                }
            }

            if (dif < 0) {
                return false; // buffer is empty
            }

            pos = head.load(std::memory_order_relaxed);
        }
    }

    // Empty and Full only offer a quick snapshot of the current ring buffer and the result may immediately expire
    // once called
    bool Empty() const
    {
        return head.load(std::memory_order_acquire) == tail.load(std::memory_order_acquire);
    }

    bool Full() const
    {
        auto t = tail.load(std::memory_order_acquire);
        auto h = head.load(std::memory_order_acquire);
        return (t - h) >= CAP;
    }

    size_t Size() const {
        return buffer.size() * sizeof(typename Container::value_type) + sizeof(head) + sizeof(tail);
    }
};

#endif
