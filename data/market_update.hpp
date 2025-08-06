#ifndef CRYPTO_TRADING_INFRA_MARKET_UPDATE
#define CRYPTO_TRADING_INFRA_MARKET_UPDATE

#include <cstdint>
#include <cstddef>
#include <arpa/inet.h>

#include "network.hpp"

namespace CryptoTradingInfra {

using Price = double;
using Size = double;

#pragma pack(1)
constexpr uint16_t PROTOCOL_MARKET_UPDATE = 0x6666;
constexpr uint16_t MAX_COUNT_MARKET_UPDATE = 20;

struct MarketUpdateHeader {
    uint16_t protocol;
    uint16_t count;

    void hton()
    {
        protocol = htons(protocol);
        count = htons(count);
    }

    void ntoh()
    {
        protocol = ntohs(protocol);
        count = ntohs(count);
    }
};

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

    void hton()
    {
        timestamp = Utils::Network::Hton64(timestamp);
        price = Utils::Network::Hton64(price);
        size = Utils::Network::Hton64(size);
    }

    void ntoh()
    {
        timestamp = Utils::Network::Ntoh64(timestamp);
        price = Utils::Network::Ntoh64(price);
        size = Utils::Network::Ntoh64(size);
    }
};

struct MarketUpdatePacket {
    MarketUpdateHeader header;
    MarketUpdate updates[];
};

constexpr std::size_t MAX_SIZE_BATCH_MARKET_UPDATE =
    sizeof(MarketUpdateHeader) + MAX_COUNT_MARKET_UPDATE * sizeof(MarketUpdate);

#pragma pack()

static_assert(sizeof(MarketUpdate) % sizeof(uint64_t) == 0, "MarketUpdate is not aligned to sizeof(uint64_t)");

}

#endif
