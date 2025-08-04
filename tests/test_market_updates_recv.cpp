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

namespace CryptoTradingInfra {
namespace Test {

constexpr size_t BUFFER_SIZE = 409600;

struct ReceiverStatus {
    uint64_t packetsRecv;
    uint64_t packetsEnqued;
    uint64_t packetsDiscarded;

    void Print()
    {
        std::cout << "Total packets received: " << packetsRecv << "\n"
                  << "Total packets enqued: " << packetsEnqued << "\n"
                  << "Total packets Discarded: " << packetsDiscarded << "\n"
                  << std::flush;
    }
};

void UdpReceiverThread(ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>& ringBuffer, std::atomic<bool>& runFlag,
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

    char buffer[sizeof(MarketUpdate)];
    while (runFlag.load(std::memory_order_relaxed)) {
        auto received = recv(sockfd, buffer, sizeof(buffer), MSG_DONTWAIT);
        ++status.packetsRecv;
        if (received == sizeof(MarketUpdate)) {
            MarketUpdate *update = reinterpret_cast<MarketUpdate *>(buffer);
            while (!ringBuffer.Emplace(update->side, update->price, update->size, update->timestamp)) {
                std::this_thread::yield();
            }
            ++status.packetsEnqued;
        } else {
            ++status.packetsDiscarded;
            std::this_thread::yield();
        }
    }
    close(sockfd);
}

std::mutex g_consumerCoutMutex;
void ConsumerThread(ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE>& ringBuffer, std::atomic<bool>& runFlag,
                    int consumerId, uint64_t& packetsProcessed)
{
    MarketUpdate update;
    while (runFlag.load(std::memory_order_relaxed)) {
        if (ringBuffer.Pop(update)) {
            ++packetsProcessed;
        } else {
            std::this_thread::yield();
        }
    }
    std::lock_guard<std::mutex> lock(g_consumerCoutMutex);
    std::cout << "[Consumer " << consumerId << "] Processed: " << packetsProcessed << " updates." << std::endl;
}

std::atomic<bool> runFlag { true };

void SignalHandler(int signum)
{
    std::cout << "\nSignal (" << signum << ") received, shutting down.\n" << std::flush;
    runFlag.store(false);
}

void TestMarketUpdatesRecv()
{
    constexpr int consumerCount = 4;
    constexpr uint16_t udpPort = 49152;

    std::signal(SIGINT, SignalHandler);

    ConcurrentRingBuffer<MarketUpdate, BUFFER_SIZE> ringBuffer;

    ReceiverStatus status { 0 };
    std::thread producer(UdpReceiverThread, std::ref(ringBuffer), std::ref(runFlag), udpPort, std::ref(status));

    std::vector<std::thread> consumers;
    uint64_t packetsProcessed[consumerCount] = { 0 };
    for (int i = 0; i < consumerCount; ++i) {
        consumers.emplace_back(ConsumerThread, std::ref(ringBuffer), std::ref(runFlag), i,
                               std::ref(packetsProcessed[i]));
    }

    std::cout << "Receiver running. Press Ctrl+C to stop...\n" << std::flush;
    // Wait until runFlag is set to false (by signal)
    while (runFlag.load()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }

    producer.join();
    for (auto& t : consumers)
        t.join();

    status.Print();
    auto totalPacketsProcessed = std::accumulate(packetsProcessed, packetsProcessed + consumerCount, 0);
    std::cout << "Total packets processed: " << totalPacketsProcessed << std::endl;
    std::cout << "Test complete." << std::endl;
}

} // namespace Test
} // namespace CryptoTradingInfra
