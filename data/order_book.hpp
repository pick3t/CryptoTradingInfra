#ifndef CRYPTO_TRADING_INFRA_ORDER_BOOK
#define CRYPTO_TRADING_INFRA_ORDER_BOOK

#include <cstddef>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <utility>

#include "market_update.hpp"

namespace CryptoTradingInfra {

class BookState
{
    static constexpr size_t MAX_DEPTH = 100;

    template <MarketUpdate::Side Side>
    std::optional<std::pair<Price, Size>> Best() const;
public:
    using Item = std::pair<Price, Size>;
    using Bids = std::map<Item::first_type, Item::second_type, std::greater<>>;
    using Asks = std::map<Item::first_type, Item::second_type, std::less<>>;

    struct BidsNAsks {
        Bids bids;
        Asks asks;

        operator Bids&();
        operator Asks&();

        operator const Bids&() const;
        operator const Asks&() const;
    } bidsNAsks;

    BookState() = default;

    BookState(const BookState& other) = default;
    BookState& operator=(const BookState& other) = default;

    // It is not allowed to move by design, thread should make a copy of the instance first
    // if it wishes to perform an update
    BookState(BookState&& other) = delete;
    BookState& operator=(BookState&& other) = delete;

    template <MarketUpdate::Side Side>
    void UpdateState(Price price, Size size);

    template <MarketUpdate::Side Side>
    bool Empty() const;

    std::optional<Item> BestBid() const;
    std::optional<Item> BestAsk() const;

    void Print(size_t depth) const;
};

class OrderBook
{
private:
    std::shared_ptr<BookState> bookState;

    using TradeHandler = std::function<void(MarketUpdate::Side, Price, Size)>;
    TradeHandler onTrade;

public:
    OrderBook();

    void UpdateOrderBook(const MarketUpdate& update);

    std::optional<BookState::Item> BestBid() const;
    std::optional<BookState::Item> BestAsk() const;

    void Print(size_t depth = 5) const;
};

} // namespace CryptoTradingInfra

#endif
