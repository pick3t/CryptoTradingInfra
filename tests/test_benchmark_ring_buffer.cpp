#include <benchmark/benchmark.h>
#include <boost/circular_buffer.hpp>
#include <thread>
#include <vector>
#include <mutex>
#include <set>
#include <cassert>
#include <condition_variable>

#include "test_entries.hpp"

namespace CryptoTradingInfra {
namespace BenchMark {

constexpr int NUM_PRODUCERS = 4;
constexpr int NUM_CONSUMERS = 4;
constexpr int ITEMS_PER_PRODUCER = 10000;
constexpr int RING_CAPACITY = 10240;

void TestBoostRingBuffer()
{
    std::mutex bufferMutex;
    std::condition_variable condition;

    std::set<int> results;

    boost::circular_buffer<int> ringBuffer(RING_CAPACITY);
    auto Producer = [](boost::circular_buffer<int>& ringBuffer, std::mutex& bufferMutex,
                       std::condition_variable& condition, int id) {
        for (auto i = 0; i < ITEMS_PER_PRODUCER; ++i) {
            std::unique_lock<std::mutex> lock(bufferMutex);
            condition.wait(lock, [&] { return !ringBuffer.full(); });

            ringBuffer.push_back(id * ITEMS_PER_PRODUCER + i);

            lock.unlock();

            condition.notify_all();
        }
    };

    auto Consumer = [&](boost::circular_buffer<int>& ringBuffer, std::mutex& bufferMutex,
                        std::condition_variable& condition, int& consumed) {
        while (true) {
            std::unique_lock<std::mutex> lock(bufferMutex);
            condition.wait(lock, [&] { return !ringBuffer.empty() || consumed == NUM_PRODUCERS * ITEMS_PER_PRODUCER; });
            if (ringBuffer.empty()) {
                if (consumed == NUM_PRODUCERS * ITEMS_PER_PRODUCER) {
                    break;
                }
                continue;
            }

            results.insert(ringBuffer.front());
            ringBuffer.pop_front();
            ++consumed;

            lock.unlock();

            condition.notify_all();
        }
    };

    std::vector<std::thread> producers;
    for (auto i = 0; i < NUM_PRODUCERS; ++i) {
        producers.emplace_back(Producer, std::ref(ringBuffer), std::ref(bufferMutex), std::ref(condition), i);
    }

    std::vector<std::thread> consumers;
    int consumed = 0;
    for (auto i = 0; i < NUM_CONSUMERS; ++i) {
        consumers.emplace_back(Consumer, std::ref(ringBuffer), std::ref(bufferMutex), std::ref(condition),
                               std::ref(consumed));
    }

    for (auto& t : producers) {
        t.join();
    }

    {
        std::lock_guard<std::mutex> lock(bufferMutex);
        condition.notify_all();
    }

    for (auto& t : consumers) {
        t.join();
    }
}

static void BenchMarkConcurrentRingBuffer(benchmark::State& state)
{
    for (auto _ : state) {
        Test::TestRingBuffer();
    }
}
BENCHMARK(BenchMarkConcurrentRingBuffer);

static void BenchMarkBoostRingBuffer(benchmark::State& state)
{
    for (auto _ : state) {
        TestBoostRingBuffer();
    }
}

BENCHMARK(BenchMarkBoostRingBuffer);

BENCHMARK_MAIN();

} // namespace BenchMark
} // namespace CryptoTradingInfra
