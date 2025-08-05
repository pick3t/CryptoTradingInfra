#include <memory>
#include <thread>

#include "execution_engine.hpp"
#include "market_update.hpp"
#include "order_book.hpp"

namespace CryptoTradingInfra {

TradingEngine::TradingEngine()
{
    std::atomic_store(&bookState, std::make_shared<BookState>());
}

void TradingEngine::print(int depth) const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    state->Print(depth);
}

std::optional<BookState::Item> TradingEngine::bestBid() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->BestBid();
}

std::optional<BookState::Item> TradingEngine::bestAsk() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->BestAsk();
}

void TradingEngine::match(const MarketUpdate& update)
{
    while (true) {
        auto oldState = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
        auto newState = std::make_shared<BookState>(*oldState);

        std::vector<MarketUpdate> trades;

        MarketUpdate::Side side = update.side;
        Price price = update.price;
        Size remaining = update.size;

        if (side == MarketUpdate::Side::BID) {
            while (remaining > 0 && !newState->Empty<MarketUpdate::Side::ASK>() &&
                   newState->BestAsk()->first <= price) {
                Price askPrice = newState->BestAsk()->first;
                Size askSize = newState->BestAsk()->second;

                Size traded = std::min(remaining, askSize);
                trades.emplace_back(MarketUpdate::Side::BID, askPrice, traded);

                if (traded == askSize) {
                    newState->UpdateState<MarketUpdate::Side::ASK>(askPrice, 0);
                } else {
                    newState->UpdateState<MarketUpdate::Side::ASK>(askPrice, -traded);
                }
                remaining -= traded;
            }

            if (remaining > 0) {
                newState->UpdateState<MarketUpdate::Side::BID>(price, remaining);
            }
        } else {
            while (remaining > 0 && !newState->Empty<MarketUpdate::Side::BID>() &&
                   newState->BestBid()->first >= price) {
                Price bidPrice = newState->BestBid()->first;
                Size bidSize = newState->BestBid()->second;

                Size traded = std::min(remaining, bidSize);
                trades.emplace_back(MarketUpdate::Side::ASK, bidPrice, traded);

                if (traded == bidSize) {
                    newState->UpdateState<MarketUpdate::Side::BID>(bidPrice, 0);
                } else {
                    newState->UpdateState<MarketUpdate::Side::BID>(bidPrice, -traded);
                }
                remaining -= traded;
            }

            if (remaining > 0) {
                newState->UpdateState<MarketUpdate::Side::ASK>(price, remaining);
            }
        }

        if (std::atomic_compare_exchange_weak_explicit(&bookState, &oldState, newState, std::memory_order_release,
                                                       std::memory_order_acquire)) {
            if (onTrade) {
                for (const auto& trade : trades) {
                    onTrade(trade.side, trade.price, trade.size);
                }
            }
            break;
        }

        std::this_thread::yield();
    }
}

} // namespace CryptoTradingInfra
