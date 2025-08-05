# Lock-Free MPMC Ring Buffer Design

## Table of Contents

1. [Introduction & Goals](#1. Introduction & Goals)
2. [Data Structure Layout](#2. Data Structure Layout)
3. [Solutions](#3. Solutions)
4. [Pseudo Code For Producer](#4. Pseudo Code For Producer)
5. [Pseudo Code For Consumer](#5. Pseudo Code For Consumer)
---

## 1. Introduction & Goals

A [ring buffer](https://en.wikipedia.org/wiki/Circular_buffer) is is a data structure that uses a single, fixed-size buffer as if it were connected end-to-end.

A **lock-free MPMC ring buffer** allows multiple threads to concurrently enqueue (produce) and dequeue (consume) items within such structure without using locks.

A ring buffer itself is not a big deal, but in order to achieve **lock-free** and **MPMC(multiple producers multiple consumers**, following challenges must be solved:

- **Race Conditions:** Multiple threads may try to access the same slot. And there are great chances for one thread to read incomplete data from the buffer while another thread is still preparing.
- **ABA Problem:** Deal with unrecognizable state change of a slot when preemption occurs. For example, thread A reads the state empty from slot X, but cpu now decides other threads get to execute their instructions first. And the state of slot X has changed from empty to full to then empty again during the rest of thread A,  thread A won't be able to detect such transition when it takes back the control.
- **Buffer Wrap-around:** This is mainly a concern of the ring buffer, but requires additional processing when concurrency kicks in.
- **False Sharing & Cache Coherency:** Different cores try to access different parts of the same cacheline, invalidating the cacheline with an update causing cache misses and significant slowdown in performance. 

---

## 2. Data Structure Layout

```cpp
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
public:
    template <typename U = T> // for perfect forwarding
  	bool push(U&&);
    bool pop(T&);
    bool empty();
    bool full();
    
    ConcurrentRingBuffer();
    // no destructor provided
    // move and copy ctors are explicitly deleted
}
```

**Template Arguments:**

- `T`: the type of item which is stored in the ring buffer
- `Capacity`: number of items which can be stored in the ring buffer, it's ceiled to the next power of 2 during compilation to deal with wrap-around
- `template <typename, typename...> class Container`: the user is able to choose either `std::array` or `std::vector` as the underlying container for the ring buffer, so that one can balance between time and efficiency considering the size of ring buffer wanted

**Members:**

- Each slot (`Node`) contains:
  - `seq`: a sequence number for ABA detection.
  - `data`: the user’s data.
- `head`: the position where the consumer should fetch the data from
- `tail`: the position where the producer should insert the data at

**Methods:**

- `push`: push data to the next available position in the buffer
- `pop`: pop the first unconsumed data from the buffer
- `empty`: tells if the buffer is currently empty. This method only provides a brief snapshot and is not guaranteed that the result stays valid after it returns, unless further synchronization steps are taken(which obvisouly defeat the purpose of using a lock-free container).
- `full`: tells if the buffer is currently full. Save as above, don't rely on the result to perform any further actions to the ring buffer.

---

## 3. Solutions

1. **Buffer Wrap-around**

    **Q**: How to handle wrap-around?

    **A**: In our ring buffer, the real capacity of the buffer is ceiled up to the next power of 2 of the given capacity. For example, if the user wants the size to be 10, it will be automatically recalculated during compilation to 16 (2^4).

    With this recalculated size, we can deal with wrap-around more efficiently than using modulo. All we need to do to get the real slot index is to mask it with (capacity - 1).

    Let's continue with the above size = 16 example, where's the slot with index = 5? 5 & (16 - 1) = 0b0101 & 0b1111 = 0b0101 = 5, this is of course what we want when the index doesn's exceed the size. What about 19, which is an index beyond the size? 19 & (16 - 1) = 0b10011 & 0b01111 = 0b00011 = 3, that's exactly the same as we expected, just the same as the result of 19 % 16 = 3.

2. **Race Conditions**

    **Q**: How to gurantee that consumer can read fully prepared data when it sees an updated flag?

    **A**: The Release-Acquire pair. C++ offers various memory order options, and one way to deal with such situation without using the old memory barrier way is to use `std::memory_order_release` paired with `std::memory_order_acquire`. `someAtomic.store(x, std::memory_order_release)` ensures all instructions related to memory changes before this operation would be visible to other threads before a `someAtomic.load(std::memory_order_release)` is performed. Thus, placing the data insertion before the store can guarantee that other threads are able to use fully constructed, complete data when they see `someAtomic` is updated.

3. **ABA Problem**

    **Q**: How to detect and avoid the ABA problem on slots in the buffer?

    **A**: Is ABA really a problem under any scenarios? IMO not necessarily, as long as the producer/consumer doesn't claim ownership of any slot in the buffer. When I say some threads own the slot, all I mean is that they might potentially store a pointer or reference to the slot, and try to use them later. 

    Imagine consumer A sees the slot X full but handles over the control to consumber B, who saw the same slot full and had a happy time with the data in it. Other consumers/producers meanwhile run infinitely until some producer filled slot X again with some other data(because a ring buffer can be wrapped around). Now consumer A finally gets to work on the rest of its unfinished job. It checks if the slot is full and without doubt succeeds. Then consumer A continues to consume the data which it was not intended to consume. However, if consumer threads all behave the same and finish consuming right away when they have access to the data(not storing a ref/pointer to the data and access it later), then there's not a problem at all with ABA occurring.

    **Q**: But we can't assume how users are going to mess around with the buffer, right?

    **A**: True. As a general container it apparently should offer solutions to the ownership problem. A typical solution to such problem is introducing a monotonic sequence number for each slot in the buffer, and the sequence number is initialized to each slot index when the buffer is constructed. Boolean flag is not enough in this case, since it saves up to 2 states(empty/full) only, which can't indicate if a wrap-around has occurred.

    By camparing the sequence number with tail/head, we can now detect if a wrap-around has happened:

    From the perspective of a producer:

    1. sequence == tail

        Tail slot it empty, proceeds to CAS. If successful, fill the data and increment the sequence by 1.

    2. sequence > tail:

        Some producer thread performed operations in 1. Therefore the producer shall find the updated tail to fill the data, current slot is already full.

    3. sequence < tail:

        Happens when producers produce faster than consumers consume and the buffer is full. Let's say the buffer's size = 4(index = 0, 1, 2, 3). The producer has filled all of them while the consumer has consumed nothing. Then the producer is trying to fill the data at tail(index = 4, wrap-around, the logical pos is actually at index 0 of the buffer), and sees the seq is still 1, producing the result sequence < tail.

    From the perspective of a consumer:

    A consumer is supposed to consume when there's data in the slot. And we increment the seq by 1 to indicate the slot is filled with data in producer's procedure 1. Therefore, we are comparing sequence with head + 1 in consumer threads.

    1. sequence == head + 1:

        Head slot is full and ready to be consumed, proceeds to CAS. If successful, consume the data and increment the sequence by the capability of the buffer. 

    2. sequence > head + 1:

        Some consumer has consumed data in the current slot, advanced the sequence number by CAP. Therefore the current consumer should try to consumer the data in the updated head.

    3. sequence < head + 1:

        This happens when the buffer is empty and the consumer has nothing to consume. For example at the initial state, no item is filled into the buffer. And the sequence number is assigned with each slot index, which is naturally smaller then any slot index + 1. 
        
        | Operation | Position (`pos`) | Real slot index    | Sequence value when ready | Sequence after operation |
        | --------- | ---------------- | ------------------ | ------------------------- | ------------------------ |
        | push      | *n*              | n & (Capacity - 1) | *n*                       | *n+1*                    |
        | pop       | *n*              | n & (Capacity - 1) | *n+1*                     | *n+Capacity*             |

4. **False Sharing & Cache Coherency**

    **Q**: How to reduce false sharing and improve performance with regard to cache coherency?

    **A**: We would like our consumers and producers have the ability to fetch their metadata on different cachelines so they wouldn't race for the same cacheline. That's what PaddedAtomic is created for. It wraps `std::atomic` inside with extra paddings, and is aligned to `std::hardware_destructive_interference_size` to make its total size equal to the cacheline size of the currently building platform. This prevents producer and consumers from constantly invalidating each other’s cache lines, which greatly enhances throughput in high-concurrency scenarios.

---

## 4. Pseudo Code For Producer

```cpp
bool push(item)
{
    while (true) {
        pos = tail;
        realSlotIndex = pos & (CAP - 1);
        node = buffer[realSlotIndex];
        dif = node.seq - pos;
        if (dif == 0) {
            if (tail.CAS(pos, pos + 1)) {
                node.data = item;
                node.seq = pos + 1;
                return true;
            }
        }

        if (dif < 0) {
            return false;
        }

        pos = tail;
    }
}
```

**Details:**

- Producer finds a slot where `seq == pos` (empty).
- Claims the slot by incrementing `tail`.
- Writes the data, then sets `seq = pos + 1` to mark full.
- If `dif < 0`, buffer is full; if `dif > 0`, retry.

---

## 5. Pseudo Code For Consumer

```cpp
bool pop(item)
{
    while (true) {
        pos = head;
        realSlotIndex = pos & (CAP - 1);
        node = buffer[realSlotIndex];
        dif = node.seq - (pos + 1);
        if (dif == 0) {
            if (head.CAS(pos, pos + 1)) {
                item = node.data;
                node.seq = pos + CAP;
                return true;
            }
        }

        if (dif < 0) {
            return false;
        }

        pos = head;
    }
}
```

**Details:**

- Consumer finds a slot where `seq == pos+1` (full).
- Claims it by incrementing `head`.
- Reads the data, then sets `seq = pos + Capacity` (ready for next lap).
- If `dif < 0`, buffer is empty; if `dif > 0`, retry.

