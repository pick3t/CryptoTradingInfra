#include "order_book.hpp"

#include <cstddef>
#include <functional>
#include <memory>
#include <atomic>
#include <optional>
#include <iostream>
#include <thread>
#include <utility>
#include <type_traits>

#include "market_update.hpp"

namespace CryptoTradingInfra {

BookState::BidsNAsks::operator Bids&()
{
    return bids;
}

BookState::BidsNAsks::operator Asks&()
{
    return asks;
}

BookState::BidsNAsks::operator const Bids&() const
{
    return bids;
}

BookState::BidsNAsks::operator const Asks&() const
{
    return asks;
}

template <MarketUpdate::Side Side>
void BookState::UpdateState(Price price, Size size)
{
    using Chosen = std::conditional_t<Side == MarketUpdate::Side::BID, Bids, Asks>;
    Chosen& side = bidsNAsks;

    // when size is 0, it's meant to remove the price level in the book
    if (size == 0.0) {
        side.erase(price);
    } else {
        side[price] += size;
    }

    if (side.size() > MAX_DEPTH) {
        side.erase(std::prev(side.end()));
    }
}

template <MarketUpdate::Side Side>
bool BookState::Empty() const
{
    using Chosen = std::conditional_t<Side == MarketUpdate::Side::BID, Bids, Asks>;
    const Chosen& side = bidsNAsks;
    return side.empty();
}

template <MarketUpdate::Side Side>
std::optional<std::pair<Price, Size>> BookState::Best() const
{
    if (Empty<Side>()) {
        return std::nullopt;
    }

    using Chosen = std::conditional_t<Side == MarketUpdate::Side::BID, Bids, Asks>;
    const Chosen& side = bidsNAsks;
    auto best = side.begin();
    return std::make_pair(best->first, best->second);
}

std::optional<BookState::Item> BookState::BestBid() const
{
    return Best<MarketUpdate::Side::BID>();
}

std::optional<BookState::Item> BookState::BestAsk() const
{
    return Best<MarketUpdate::Side::ASK>();
}

void BookState::Print(size_t depth) const
{
    depth = std::min(depth, MAX_DEPTH);

    std::cout << "Asks:\n";
    int count = 0;
    for (auto& [price, size] : bidsNAsks.asks) {
        std::cout << price << " @" << size << "\n";
        if (++count >= depth) {
            break;
        }
    }
    std::cout << "Bids:\n";
    count = 0;
    for (auto& [price, size] : bidsNAsks.bids) {
        std::cout << price << " @" << size << "\n";
        if (++count >= depth) {
            break;
        }
    }
}

OrderBook::OrderBook()
{
    std::atomic_store(&bookState, std::make_shared<BookState>());
}

void OrderBook::UpdateOrderBook(const MarketUpdate& update)
{
    while (true) {
        auto oldState = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
        auto newState = std::make_shared<BookState>(*oldState);

        std::vector<std::tuple<MarketUpdate::Side, Price, Size>> trades;

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
                    onTrade(std::get<0>(trade), std::get<1>(trade), std::get<2>(trade));
                }
            }
            break;
        }

        std::this_thread::yield();
    }
}

std::optional<std::pair<Price, Size>> OrderBook::BestBid() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->BestBid();
}

std::optional<std::pair<Price, Size>> OrderBook::BestAsk() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->BestAsk();
}

void OrderBook::Print(size_t depth) const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    state->Print(depth);
}

} // namespace CryptoTradingInfra
