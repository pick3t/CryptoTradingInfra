#include "test_entries.hpp"

int main()
{
    CryptoTradingInfra::Test::TestRingBuffer();
    CryptoTradingInfra::Test::TestOrderBookBasic();
    CryptoTradingInfra::Test::TestExecutionEngineBasic();
    CryptoTradingInfra::Test::TestExecutionEngineCrossTrades();

    // Uncomment to test receiving udp pakcets containing MarketUpdates from port 49152
    // You may use the udp_market_client.py script to generate packets
    /* CryptoTradingInfra::Test::TestMarketUpdatesRecv(); */
    return 0;
}
