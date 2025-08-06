#include <atomic>
#include <cstdint>
#include <cstring>
#include <functional>
#include <iostream>
#include <mutex>
#include <numeric>
#include <thread>
#include <vector>
#include <chrono>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <cassert>
#include <csignal>

#include "market_update.hpp"
#include "ring_buffer.hpp"
#include "order_book.hpp"

namespace CryptoTradingInfra {
namespace Test {

constexpr size_t BUFFER_SIZE = 4096000;

struct ReceiverStatus {
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

void UdpReceiverThread(Utils::ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>& ringBuffer, std::atomic<bool>& g_runFlag,
                       uint16_t port, ReceiverStatus& status)
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
    while (g_runFlag.load(std::memory_order_relaxed)) {
        auto received = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT);
        if (received < sizeof(MarketUpdateHeader)) {
            ++status.packetsDiscarded;
            std::this_thread::yield();
            continue;
        }

        auto packet = reinterpret_cast<MarketUpdatePacket*>(buffer);
        auto header = &packet->header;
        header->ntoh();
        if (header->protocol != PROTOCOL_MARKET_UPDATE || header->count > MAX_COUNT_MARKET_UPDATE ||
            received != sizeof(MarketUpdateHeader) + header->count * sizeof(MarketUpdate)) {
            ++status.packetsDiscarded;
            std::this_thread::yield();
            continue;
        }

        ++status.packetsRecv;

        for (auto i = 0; i < header->count; ++i) {
            packet->updates[i].ntoh();
            const auto& update = packet->updates[i];
            while (!ringBuffer.emplace(update.side, update.price, update.size, update.timestamp)) {
                std::this_thread::yield();
            }
            ++status.packetsEnqued;
        }
    }
    close(sockfd);
}

std::mutex g_consumerCoutMutex;
OrderBook g_orderBook;
void ConsumerThread(Utils::ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>& ringBuffer, std::atomic<bool>& g_runFlag,
                    int consumerId, uint64_t& packetsProcessed)
{
    MarketUpdate update;
    while (g_runFlag.load(std::memory_order_relaxed)) {
        if (ringBuffer.pop(update)) {
            ++packetsProcessed;
            g_orderBook.updateOrderBook(update);
        } else {
            std::this_thread::yield();
        }
    }
}

std::atomic<bool> g_runFlag { true };

void SignalHandler(int signum)
{
    std::cout << "\nSignal (" << signum << ") received, shutting down.\n" << std::flush;
    g_runFlag.store(false);
}

void TestMarketUpdatesRecv()
{
    constexpr int consumerCount = 4;
    constexpr uint16_t udpPort = 49152;

    std::signal(SIGINT, SignalHandler);

    Utils::ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE> ringBuffer;
    std::cout << "The size of ring buffer used is " << (static_cast<double>(ringBuffer.size()) / (1024.0 * 1024.0))
              << "MB." << std::endl;

    ReceiverStatus status { 0 };
    std::thread producer(UdpReceiverThread, std::ref(ringBuffer), std::ref(g_runFlag), udpPort, std::ref(status));

    std::vector<std::thread> consumers;
    uint64_t packetsProcessed[consumerCount] = { 0 };
    for (int i = 0; i < consumerCount; ++i) {
        consumers.emplace_back(ConsumerThread, std::ref(ringBuffer), std::ref(g_runFlag), i,
                               std::ref(packetsProcessed[i]));
    }

    std::cout << "Receiver running. Press Ctrl+C to stop...\n" << std::flush;
    // Wait until g_runFlag is set to false (by signal)
    while (g_runFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    producer.join();
    for (auto& t : consumers)
        t.join();

    status.print();
    auto totalPacketsProcessed = std::accumulate(packetsProcessed, packetsProcessed + consumerCount, 0);
    std::cout << "Total packets processed: " << totalPacketsProcessed << std::endl;
    std::cout << "Test complete." << std::endl;
    g_orderBook.print();
}

} // namespace Test
} // namespace CryptoTradingInfra
