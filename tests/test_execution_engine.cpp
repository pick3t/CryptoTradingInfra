#include "test_entries.hpp"

#include <optional>
#include <cassert>

#include "market_update.hpp"
#include "order_book.hpp"
#include "execution_engine.hpp"

namespace CryptoTradingInfra {
namespace Test {

void TestExecutionEngineBasic() {
    TradingEngine engine;

    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 101, 10});
    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 102, 20});
    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 103, 30});
    // Add some bids at 100, 99, 98
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 100, 5});
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 99, 10});
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 98, 15});

    assert(engine.bestAsk() == std::make_pair(101.0, 10.0));
    assert(engine.bestBid() == std::make_pair(100.0, 5.0));

    engine.print();
}

void TestExecutionEngineCrossTrades() {
    TradingEngine engine;

    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 105, 10});
    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 106, 20});

    engine.match(MarketUpdate{MarketUpdate::Side::BID, 104, 5});
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 103, 10});

    engine.match(MarketUpdate{MarketUpdate::Side::BID, 105, 7});
    // Should match against 105@10, so book should now have 105@3 (ask side), 105 not present on bid side

    auto ask = engine.bestAsk();
    auto bid = engine.bestBid();
    assert(ask == std::make_pair(105.0, 3.0)); // 10 - 7 = 3 left
    assert(bid == std::make_pair(104.0, 5.0)); // unchanged

    // Another bid 105@4, should trade against remaining 3 at 105, and put 1 at bid side at 105
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 105, 4});
    ask = engine.bestAsk();
    bid = engine.bestBid();
    assert(ask == std::make_pair(106.0, 20.0)); // 105 ask is gone
    assert(bid == std::make_pair(105.0, 1.0));  // only 1 remains at bid side

    // Add ask at 104, which will cross the 105@1 bid
    engine.match(MarketUpdate{MarketUpdate::Side::ASK, 104, 2});
    // 1 trade at 105, 1 trade at 104, leaving none at 104 ask side, and bid side should be 104@4
    ask = engine.bestAsk();
    bid = engine.bestBid();
    assert(ask == std::make_pair(106.0, 20.0)); // 2 - 1 = 1 left
    assert(bid == std::make_pair(104.0, 4.0)); // next best

    // Now consume ask at 106 completely
    engine.match(MarketUpdate{MarketUpdate::Side::BID, 106, 21});
    ask = engine.bestAsk();
    bid = engine.bestBid();
    engine.print();
    assert(ask == std::nullopt);
    assert(bid == std::make_pair(106.0, 1.0));
}

} // namespace Test

} // namespace CryptoTradingInfra
