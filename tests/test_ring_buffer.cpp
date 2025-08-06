#include <iostream>
#include <thread>
#include <vector>
#include <set>
#include <mutex>
#include <cassert>

#include "ring_buffer.hpp"

namespace CryptoTradingInfra {
namespace Test {

constexpr int NUM_PRODUCERS = 4;
constexpr int NUM_CONSUMERS = 4;
constexpr int ITEMS_PER_PRODUCER = 10000;
constexpr int RING_CAPACITY = 10240;

void Producer(Utils::ConcurrentRingBuffer<int, RING_CAPACITY>& buffer, std::mutex& resultsMutex, int id) {
    int base = id * ITEMS_PER_PRODUCER;
    for (int i = 0; i < ITEMS_PER_PRODUCER; ++i) {
        int value = base + i;
        if (i < 2) {
            while (!buffer.push(value)) {
                std::this_thread::yield();
            }
        } else {
            while (!buffer.emplace(value)) {
                std::this_thread::yield();
            }
        }
    }
}

void Consumer(Utils::ConcurrentRingBuffer<int, RING_CAPACITY>& buffer, std::set<int>& results, std::mutex& resultsMutex) {
    int value;
    while (true) {
        if (buffer.pop(value)) {
            std::lock_guard<std::mutex> lock(resultsMutex);
            results.insert(value);
        } else {
            if (results.size() >= NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                break;
            }
            std::this_thread::yield();
        }
    }
}

void TestRingBuffer() {
#ifndef CRYPTO_TRADING_INFRA_BENCHMARK
    std::cout << "Test started. " << std::endl;
#endif
    std::set<int> results;
    std::mutex resultsMutex;
    Utils::ConcurrentRingBuffer<int, RING_CAPACITY> buffer;

    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(Producer, std::ref(buffer), std::ref(resultsMutex), i);
    }

    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(Consumer, std::ref(buffer), std::ref(results), std::ref(resultsMutex));
    }

    for (auto& t : producers) {
        t.join();
    }

    for (auto& t : consumers) {
        t.join();
    }

#ifndef CRYPTO_TRADING_INFRA_BENCHMARK
    assert(results.size() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS * ITEMS_PER_PRODUCER; ++i) {
        if (results.find(i) == results.end()) {
            std::cout << "Missing value: " << i << std::endl;
        }
    }
    std::cout << "Test completed. " << results.size() << " items popped." << std::endl;
#endif
}

}
}
