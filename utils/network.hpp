#ifndef CRYPTO_TRADING_INFRA_UTILS_NETWORK
#define CRYPTO_TRADING_INFRA_UTILS_NETWORK

#include <cstdint>
#include <type_traits>
#include <cstring>
#include <arpa/inet.h>

namespace CryptoTradingInfra {
namespace Utils {
namespace Network {

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#define IS_LITTLE_ENDIAN 1
#else
#define IS_LITTLE_ENDIAN 0
#endif

template <typename T>
T Hton64(T value)
{
    static_assert(sizeof(T) == sizeof(uint64_t), "hton64 requires 64-bit type");
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

#if IS_LITTLE_ENDIAN
    uint64_t temp;
    std::memcpy(&temp, &value, sizeof(T));
    temp = (static_cast<uint64_t>(htonl(temp & 0xFFFFFFFF)) << 32) | htonl(temp >> 32);
    T result;
    std::memcpy(&result, &temp, sizeof(T));
    return result;
#else
    return value;
#endif
}

template <typename T>
T Ntoh64(T value)
{
    static_assert(sizeof(T) == sizeof(uint64_t), "ntoh64 requires 64-bit type");
    static_assert(std::is_trivially_copyable<T>::value, "T must be trivially copyable");

#if IS_LITTLE_ENDIAN
    uint64_t temp;
    std::memcpy(&temp, &value, sizeof(T));
    temp = (static_cast<uint64_t>(ntohl(temp & 0xFFFFFFFF)) << 32) | ntohl(temp >> 32);
    T result;
    std::memcpy(&result, &temp, sizeof(T));
    return result;
#else
    return value;
#endif
}

} // namespace Network
} // namespace Utils
} // namespace CryptoTradingInfra

#endif
