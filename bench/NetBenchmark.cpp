#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <csignal>
#include <numeric>
#include <vector>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "matchbox/net/SocketUtil.hpp"
#include "matchbox/net/WireProtocol.hpp"

using namespace matchbox::net;

namespace {

using Clock = std::chrono::steady_clock;

[[noreturn]] void die(const char* msg) {
    std::perror(msg);
    std::exit(1);
}

int connectTo(const char* host, int port) {
    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ::inet_addr(host);
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) die("connect");
    return fd;
}

// One synchronous round trip: send a new-order message, then block until the
// ack arrives. Returns the elapsed send->ack time in nanoseconds.
long roundTrip(int fd, int64_t price) {
    NewOrderMessage msg{static_cast<uint8_t>(MessageType::NEW_ORDER), 0, 0, price, 10};
    auto t0 = Clock::now();
    if (sendExact(fd, &msg, sizeof(msg)) < 0) die("send");
    NewOrderAck ack{};
    if (recvExact(fd, &ack, sizeof(ack)) <= 0) die("recv ack");
    auto t1 = Clock::now();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();
}

long percentile(std::vector<long> v, double p) {
    std::sort(v.begin(), v.end());
    return v[static_cast<std::size_t>(p * static_cast<double>(v.size() - 1))];
}

}  // namespace

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 9100;
    int64_t warmup = argc > 2 ? std::atoll(argv[2]) : 1000;
    int64_t orders = argc > 3 ? std::atoll(argv[3]) : 10000;

    std::signal(SIGPIPE, SIG_IGN);

    int fd = connectTo("127.0.0.1", port);
    std::printf("matchbox_netbench: connected to 127.0.0.1:%d\n", port);

    // Warmup: send some orders and wait for their acks so the connection and
    // allocator state settle before we start the clock.
    for (int64_t i = 0; i < warmup; ++i) roundTrip(fd, 100'000 + i);

    std::vector<long> latencies;
    latencies.reserve(static_cast<std::size_t>(orders));
    auto t0 = Clock::now();
    for (int64_t i = 0; i < orders; ++i)
        latencies.push_back(roundTrip(fd, 200'000 + i));
    auto t1 = Clock::now();

    double elapsedSec =
        std::chrono::duration<double>(t1 - t0).count();
    double ordersPerSec = static_cast<double>(orders) / elapsedSec;

    double totalNanos =
        static_cast<double>(std::accumulate(latencies.begin(), latencies.end(), 0L));
    auto [minIt, maxIt] = std::minmax_element(latencies.begin(), latencies.end());

    std::printf("--- matchbox_netbench results ---\n");
    std::printf("Orders sent:         %ld (warmup %ld)\n", orders, warmup);
    std::printf("Elapsed:             %.3f sec\n", elapsedSec);
    std::printf("Throughput:          %.0f orders/sec\n", ordersPerSec);
    std::printf("Round-trip latency (ns):\n");
    std::printf("  avg:               %.0f\n", totalNanos / static_cast<double>(orders));
    std::printf("  min:               %ld\n", *minIt);
    std::printf("  p50:               %ld\n", percentile(latencies, 0.50));
    std::printf("  p99:               %ld\n", percentile(latencies, 0.99));
    std::printf("  max:               %ld\n", *maxIt);

    ::close(fd);
    return 0;
}
