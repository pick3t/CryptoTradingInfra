#ifndef CRYPTO_TRADING_INFRA_MATH
#define CRYPTO_TRADING_INFRA_MATH

#include <cstdint>

template <uint64_t N>
constexpr uint64_t NextPowerOf2() {
    auto n = N;
    if (n <= 1) return 1;
    n--;
    n |= n >> 1;
    n |= n >> 2;
    n |= n >> 4;
    n |= n >> 8;
    n |= n >> 16;
    n |= n >> 32;
    return n + 1;
}

#endif
