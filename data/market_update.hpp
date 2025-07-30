#ifndef CRYPTO_TRADING_INFRA_MARKET_UPDATE
#define CRYPTO_TRADING_INFRA_MARKET_UPDATE

#include <cstdint>

#pragma pack(1)
struct MarketUpdate {
    enum class Side : uint8_t {
        ASK = 0,
        BID = 1,
    };

    uint64_t timestamp;
    double price;
    double size;
    Side side;
    char resv[sizeof(uint64_t) - sizeof(side)];

    MarketUpdate() = default;

    MarketUpdate(uint64_t timestamp, double price, double size, Side side) : resv{}
    {
        this->timestamp = timestamp;
        this->price = price;
        this->size = size;
        this->side = side;
    }
};
#pragma pack()

static_assert(sizeof(MarketUpdate) % sizeof(uint64_t) == 0, "MarketUpdate is not aligned to sizeof(uint64_t)");

#endif
