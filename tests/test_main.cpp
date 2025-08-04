#include "test_entries.hpp"

int main()
{
    CryptoTradingInfra::Test::TestRingBuffer();
    CryptoTradingInfra::Test::TestOrderBookBasic();
    CryptoTradingInfra::Test::TestOrderBookCrossTrades();
    CryptoTradingInfra::Test::TestOrderBookMultiThreads();

    CryptoTradingInfra::Test::TestMarketUpdatesRecv();
    return 0;
}
