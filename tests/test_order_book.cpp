#include "test_entries.hpp"

#include <optional>
#include <random>
#include <thread>
#include <iostream>
#include <cassert>

#include "market_update.hpp"
#include "order_book.hpp"

namespace CryptoTradingInfra {
namespace Test {

void TestOrderBookBasic() {
    OrderBook ob;

    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 101, 10});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 102, 20});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 103, 30});
    // Add some bids at 100, 99, 98
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 100, 5});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 99, 10});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 98, 15});

    assert(ob.BestAsk() == std::make_pair(101.0, 10.0));
    assert(ob.BestBid() == std::make_pair(100.0, 5.0));

    ob.Print();
}

void TestOrderBookCrossTrades() {
    OrderBook ob;

    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 105, 10});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 106, 20});

    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 104, 5});
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 103, 10});

    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 105, 7});
    // Should match against 105@10, so book should now have 105@3 (ask side), 105 not present on bid side

    auto ask = ob.BestAsk();
    auto bid = ob.BestBid();
    assert(ask == std::make_pair(105.0, 3.0)); // 10 - 7 = 3 left
    assert(bid == std::make_pair(104.0, 5.0)); // unchanged

    // Another bid 105@4, should trade against remaining 3 at 105, and put 1 at bid side at 105
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 105, 4});
    ask = ob.BestAsk();
    bid = ob.BestBid();
    assert(ask == std::make_pair(106.0, 20.0)); // 105 ask is gone
    assert(bid == std::make_pair(105.0, 1.0));  // only 1 remains at bid side

    // Add ask at 104, which will cross the 105@1 bid
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::ASK, 104, 2});
    // 1 trade at 105, 1 trade at 104, leaving none at 104 ask side, and bid side should be 104@4
    ask = ob.BestAsk();
    bid = ob.BestBid();
    assert(ask == std::make_pair(106.0, 20.0)); // 2 - 1 = 1 left
    assert(bid == std::make_pair(104.0, 4.0)); // next best

    // Now consume ask at 106 completely
    ob.UpdateOrderBook(MarketUpdate{MarketUpdate::Side::BID, 106, 21});
    ask = ob.BestAsk();
    bid = ob.BestBid();
    ob.Print();
    assert(ask == std::nullopt);
    assert(bid == std::make_pair(106.0, 1.0));
}

void TestOrderBookMultiThreads()
{
    OrderBook book;

    constexpr int NUM_WRITERS = 8;
    constexpr int NUM_READERS = 4;
    constexpr int UPDATES_PER_WRITER = 200;

    std::atomic<bool> stop { false };

    auto writer = [&](int id) {
        std::mt19937 rng(std::random_device {}());
        std::uniform_real_distribution<> randPrice(90, 110);
        std::uniform_int_distribution<> randSize(1, 100);
        std::uniform_int_distribution<> randSide(0, 1);

        for (int i = 0; i < UPDATES_PER_WRITER; ++i) {
            book.UpdateOrderBook(
                MarketUpdate(static_cast<MarketUpdate::Side>(randSide(rng)), randPrice(rng), randSize(rng)));
            if (i % 100 == 0) {
                std::this_thread::yield();
            }
        }
    };

    auto reader = [&]() {
        while (!stop.load()) {
            [[maybe_unused]] auto bid = book.BestBid();
            [[maybe_unused]] auto ask = book.BestAsk();
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
    book.Print(10);
}
} // namespace Test

} // namespace CryptoTradingInfra
