#include "test_entries.hpp"

#include <random>
#include <thread>
#include <iostream>
#include <cassert>
#include <atomic>

#include "market_update.hpp"
#include "order_book.hpp"

namespace CryptoTradingInfra {
namespace Test {

void TestOrderBook()
{
    OrderBook book;

    constexpr int NUM_WRITERS = 4;
    constexpr int NUM_READERS = 4;
    constexpr int UPDATES_PER_WRITER = 200;

    std::atomic<bool> stop { false };

    auto writer = [&](int id) {
        std::mt19937 rng(std::random_device {}());
        std::uniform_real_distribution<> randPrice(90, 110);
        std::uniform_int_distribution<> randSize(1, 100);
        std::uniform_int_distribution<> randSide(0, 1);

        for (int i = 0; i < UPDATES_PER_WRITER; ++i) {
            book.updateOrderBook(
                MarketUpdate(static_cast<MarketUpdate::Side>(randSide(rng)), randPrice(rng), randSize(rng)));
            if (i % 100 == 0) {
                std::this_thread::yield();
            }
        }
    };

    auto reader = [&]() {
        while (!stop.load()) {
            [[maybe_unused]] auto bid = book.bestBid();
            [[maybe_unused]] auto ask = book.bestAsk();
            std::this_thread::yield();
        }
    };

    std::vector<std::thread> writers;
    for (int i = 0; i < NUM_WRITERS; ++i) {
        writers.emplace_back(writer, i);
    }

    std::vector<std::thread> readers;
    for (int i = 0; i < NUM_READERS; ++i) {
        readers.emplace_back(reader);
    }

    for (auto& t : writers) {
        if (t.joinable()) {
            t.join();
        }
    }

    stop.store(true);
    for (auto& t : readers) {
        if (t.joinable()) {
            t.join();
        }
    }

    std::cout << "Final OrderBook:\n";
    book.print(10);
}
} // namespace Test

} // namespace CryptoTradingInfra
