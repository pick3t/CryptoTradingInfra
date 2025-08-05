#include "test_entries.hpp"

int main()
{
    CryptoTradingInfra::Test::TestRingBuffer();
    CryptoTradingInfra::Test::TestOrderBookBasic();
    CryptoTradingInfra::Test::TestExecutionEngineBasic();
    CryptoTradingInfra::Test::TestExecutionEngineCrossTrades();

    CryptoTradingInfra::Test::TestMarketUpdatesRecv();
    return 0;
}
