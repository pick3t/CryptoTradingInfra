#ifndef CRYPTO_TRADING_INFRA_MARKET_UPDATE
#define CRYPTO_TRADING_INFRA_MARKET_UPDATE

#include <cstdint>

namespace CryptoTradingInfra {

using Price = double;
using Size = double;

#pragma pack(1)
struct MarketUpdate {
    enum class Side : uint8_t {
        ASK = 0,
        BID = 1,
    };

    uint64_t timestamp;
    Price price;
    Size size;
    Side side;
    char resv[sizeof(uint64_t) - sizeof(side)];

    MarketUpdate() = default;

    MarketUpdate(Side side, Price price, Size size, uint64_t timestamp = 0) : resv{0}
    {
        this->timestamp = timestamp;
        this->price = price;
        this->size = size;
        this->side = side;
    }
};
#pragma pack()

static_assert(sizeof(MarketUpdate) % sizeof(uint64_t) == 0, "MarketUpdate is not aligned to sizeof(uint64_t)");

}

#endif
