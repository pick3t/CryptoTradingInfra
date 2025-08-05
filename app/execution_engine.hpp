#ifndef CRYPTO_TRADING_INFRA_EXECUTION_ENGINE
#define CRYPTO_TRADING_INFRA_EXECUTION_ENGINE

#include <memory>
#include <functional>

#include "market_update.hpp"
#include "order_book.hpp"

namespace CryptoTradingInfra {

class TradingEngine {
private:
    std::shared_ptr<BookState> bookState;

    using TradeHandler = std::function<void(MarketUpdate::Side, Price, Size)>;
    TradeHandler onTrade;

public:
    TradingEngine();

    std::optional<BookState::Item> bestBid() const;
    std::optional<BookState::Item> bestAsk() const;

    void match(const MarketUpdate& update);

    void print(int depth = 5) const;
};

} // namespace CryptoTradingInfra

#endif
