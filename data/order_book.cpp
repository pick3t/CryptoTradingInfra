#include "order_book.hpp"

#include <cstddef>
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
void BookState::updateState(Price price, Size size)
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
bool BookState::empty() const
{
    using Chosen = std::conditional_t<Side == MarketUpdate::Side::BID, Bids, Asks>;
    const Chosen& side = bidsNAsks;
    return side.empty();
}

template <MarketUpdate::Side Side>
std::optional<std::pair<Price, Size>> BookState::Best() const
{
    if (empty<Side>()) {
        return std::nullopt;
    }

    using Chosen = std::conditional_t<Side == MarketUpdate::Side::BID, Bids, Asks>;
    const Chosen& side = bidsNAsks;
    auto best = side.begin();
    return std::make_pair(best->first, best->second);
}

std::optional<BookState::Item> BookState::bestBid() const
{
    return Best<MarketUpdate::Side::BID>();
}

std::optional<BookState::Item> BookState::bestAsk() const
{
    return Best<MarketUpdate::Side::ASK>();
}

void BookState::print(size_t depth) const
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

void OrderBook::updateOrderBook(const MarketUpdate& update)
{
    while (true) {
        auto oldState = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
        auto newState = std::make_shared<BookState>(*oldState);

        MarketUpdate::Side side = update.side;
        Price price = update.price;
        Size remaining = update.size;

        if (side == MarketUpdate::Side::BID) {
            newState->updateState<MarketUpdate::Side::BID>(update.price, update.size);
        } else {
            newState->updateState<MarketUpdate::Side::ASK>(update.price, update.size);
        }

        if (std::atomic_compare_exchange_weak_explicit(&bookState, &oldState, newState, std::memory_order_release,
                                                       std::memory_order_acquire)) {
            break;
        }

        std::this_thread::yield();
    }
}

std::optional<BookState::Item> OrderBook::bestBid() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->bestBid();
}

std::optional<BookState::Item> OrderBook::bestAsk() const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    return state->bestAsk();
}

void OrderBook::print(size_t depth) const
{
    auto state = std::atomic_load_explicit(&bookState, std::memory_order_acquire);
    state->print(depth);
}

} // namespace CryptoTradingInfra
