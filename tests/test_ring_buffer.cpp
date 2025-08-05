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
constexpr int RING_CAPACITY = 1024;

ConcurrentRingBuffer<int, RING_CAPACITY> buffer;

// A thread-safe set to collect popped values
std::set<int> results;
std::mutex results_mutex;

void Producer(int id) {
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

void Consumer() {
    int value;
    while (true) {
        if (buffer.pop(value)) {
            std::lock_guard<std::mutex> lock(results_mutex);
            results.insert(value);
        } else {
            // We use the size of results to decide when to stop
            if (results.size() >= NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                break;
            }
            std::this_thread::yield();
        }
    }
}

void TestRingBuffer() {
    std::cout << "Test started. " << std::endl;
    std::vector<std::thread> producers;
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(Producer, i);
    }

    std::vector<std::thread> consumers;
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(Consumer);
    }

    for (auto& t : producers) {
        t.join();
    }

    for (auto& t : consumers) {
        t.join();
    }

    assert(results.size() == NUM_PRODUCERS * ITEMS_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS * ITEMS_PER_PRODUCER; ++i) {
        if (results.find(i) == results.end()) {
            std::cout << "Missing value: " << i << std::endl;
        }
    }
    std::cout << "Test completed. " << results.size() << " items popped." << std::endl;
}

}
}
