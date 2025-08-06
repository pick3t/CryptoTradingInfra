#include <cstdint>
#include <ostream>
#include <string>
#include <atomic>
#include <iostream>
#include <numeric>
#include <thread>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <csignal>

#include "math.hpp"
#include "network.hpp"
#include "ring_buffer.hpp"
#include "order_book.hpp"
#include "execution_engine.hpp"

#if __cplusplus < 201703L
#error "C++17 standard support required."
#endif

namespace CryptoTradingInfra {

constexpr std::size_t BUFFER_SIZE = 4096000;

using OrderBookBuffer = Utils::ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>;
using TradingEngineBuffer = Utils::ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>;

OrderBook g_orderBook;
TradingEngine g_tradingEngine;

struct PacketStats {
    uint64_t packetsRecv;
    uint64_t packetsEnqued;
    uint64_t packetsDiscarded;

    void print()
    {
        std::cout << "Total packets received: " << packetsRecv << "\n"
                  << "Total packets enqued: " << packetsEnqued << "\n"
                  << "Total packets Discarded: " << packetsDiscarded << "\n"
                  << std::flush;
    }
};

void ReceiveMarketUpdate(std::atomic<bool>& runFlag, OrderBookBuffer& orderBookBuffer,
                         TradingEngineBuffer& tradingEngineBuffer, uint16_t port, PacketStats& stats)
{
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return;
    }

    sockaddr_in addr {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port);

    if (bind(sockfd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr)) < 0) {
        perror("bind");
        close(sockfd);
        return;
    }

    std::cout << "Port " << port << " is listening\n" << std::flush;

    char buffer[MAX_SIZE_BATCH_MARKET_UPDATE];
    while (runFlag.load(std::memory_order_relaxed)) {
        auto received = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (received < sizeof(MarketUpdateHeader)) {
            ++stats.packetsDiscarded;
            std::this_thread::yield();
            continue;
        }

        auto packet = reinterpret_cast<MarketUpdatePacket *>(buffer);
        auto header = &packet->header;
        header->ntoh();
        if (header->protocol != PROTOCOL_MARKET_UPDATE || header->count > MAX_COUNT_MARKET_UPDATE ||
            received != sizeof(MarketUpdateHeader) + header->count * sizeof(MarketUpdate)) {
            ++stats.packetsDiscarded;
            std::this_thread::yield();
            continue;
        }

        ++stats.packetsRecv;

        for (auto i = 0; i < header->count; ++i) {
            packet->updates[i].ntoh();
            const auto& update = packet->updates[i];
            while (!orderBookBuffer.emplace(update.side, update.price, update.size, update.timestamp)) {
                std::this_thread::yield();
            }
            while (!tradingEngineBuffer.emplace(update.side, update.price, update.size, update.timestamp)) {
                std::this_thread::yield();
            }
            ++stats.packetsEnqued;
        }
    }
    close(sockfd);
}

void Publish2OrderBook(std::atomic<bool>& runFlag, OrderBookBuffer& orderBookBuffer, uint64_t& updatesProcessed)
{
    while (runFlag.load(std::memory_order_relaxed)) {
        MarketUpdate update;
        if (orderBookBuffer.pop(update)) {
            ++updatesProcessed;
            g_orderBook.updateOrderBook(update);
        } else {
            std::this_thread::yield();
        }
    }
}

void Publish2TradingEngine(std::atomic<bool>& runFlag, TradingEngineBuffer& tradingEngineBuffer,
                           uint64_t& tradesProcessed)
{
    while (runFlag.load(std::memory_order_relaxed)) {
        MarketUpdate update;
        if (tradingEngineBuffer.pop(update)) {
            ++tradesProcessed;
            g_tradingEngine.match(update);
        } else {
            std::this_thread::yield();
        }
    }
}

} // namespace CryptoTradingInfra

std::atomic<bool> g_runFlag { true };
void SignalHandler(int signum)
{
    std::cout << "\nSignal (" << signum << ") received, shutting down.\n" << std::flush;
    g_runFlag.store(false);
}

int main(int argc, char *argv[])
{
    auto isValidUdpPort = [](int port) { return port >= 49152 && port <= 65535; };
    uint16_t port = 49152;
    if (argc >= 2) {
        try {
            auto parsed = std::stoi(argv[1]);
            if (!isValidUdpPort(parsed)) {
                std::cerr << "Error: You should choose a port between 49152 and 65535.\n" << std::flush;
                return 1;
            }
            port = static_cast<uint16_t>(parsed);
        } catch (...) {
            std::cerr << "Usage: " << argv[0] << " [UDP_PORT]\n";
            std::cerr << "UDP_PORT must be between 49152 and 65535 (default is 49152).\n" << std::flush;
            return 1;
        }
    }

    std::signal(SIGINT, SignalHandler);
    std::cout << "Engine running. Press Ctrl+C to stop...\n" << std::flush;

    CryptoTradingInfra::OrderBookBuffer orderBookBuffer;
    constexpr int orderBookPublishersNum = 4;
    std::vector<std::thread> orderBookPublishers;
    uint64_t updatesProcessed[orderBookPublishersNum] = { 0 };
    for (auto i = 0; i < orderBookPublishersNum; ++i) {
        orderBookPublishers.emplace_back(CryptoTradingInfra::Publish2OrderBook, std::ref(g_runFlag),
                                         std::ref(orderBookBuffer), std::ref(updatesProcessed[i]));
    }

    CryptoTradingInfra::TradingEngineBuffer tradingEngineBuffer;
    constexpr int tradingEnginePublishersNum = 4;
    std::vector<std::thread> tradingEnginePublishers;
    uint64_t tradesProcessed[tradingEnginePublishersNum] = { 0 };
    for (auto i = 0; i < tradingEnginePublishersNum; ++i) {
        tradingEnginePublishers.emplace_back(CryptoTradingInfra::Publish2TradingEngine, std::ref(g_runFlag),
                                             std::ref(tradingEngineBuffer), std::ref(tradesProcessed[i]));
    }

    CryptoTradingInfra::PacketStats stats { 0 };
    std::thread marketUpdatesReceiver(CryptoTradingInfra::ReceiveMarketUpdate, std::ref(g_runFlag),
                                      std::ref(orderBookBuffer), std::ref(tradingEngineBuffer), port, std::ref(stats));


    while (g_runFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    marketUpdatesReceiver.join();
    for (auto& t : orderBookPublishers) {
        t.join();
    }

    for (auto& t : tradingEnginePublishers) {
        t.join();
    }

    // printing stats
    stats.print();
    auto totalUpdatesProcessed = std::accumulate(updatesProcessed, updatesProcessed + orderBookPublishersNum, 0);
    auto totalTradesProcessed = std::accumulate(tradesProcessed, tradesProcessed + tradingEnginePublishersNum, 0);
    std::cout << "Total MarketUpdates processed: " << totalUpdatesProcessed << std::endl;
    std::cout << "Total Trades processed:        " << totalTradesProcessed << std::endl;
    CryptoTradingInfra::g_orderBook.print();
    CryptoTradingInfra::g_tradingEngine.print();
}
