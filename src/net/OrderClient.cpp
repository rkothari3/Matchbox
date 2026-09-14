#include <csignal>
#include <cstdio>
#include <cstdlib>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "matchbox/net/SocketUtil.hpp"
#include "matchbox/net/WireProtocol.hpp"

using namespace matchbox::net;

namespace {

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

// Send one new-order message, read the matching ack, and print both sides so
// you can eyeball that the gateway understood each order.
void sendOrder(int fd, uint8_t side, uint8_t tif, int64_t price, int64_t quantity) {
    NewOrderMessage msg{static_cast<uint8_t>(MessageType::NEW_ORDER), side, tif, price, quantity};
    if (sendExact(fd, &msg, sizeof(msg)) < 0) die("send");

    NewOrderAck ack{};
    if (recvExact(fd, &ack, sizeof(ack)) <= 0) die("recv ack");

    std::printf("  sent  side=%u tif=%u price=%ld qty=%ld\n", side, tif, price, quantity);
    std::printf("  ack   success=%u orderId=%ld\n", ack.success, ack.orderId);
}

}  // namespace

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 9100;

    // Ignore SIGPIPE too: if the server died, send() should fail and we exit
    // cleanly instead of the process being killed silently.
    std::signal(SIGPIPE, SIG_IGN);

    int fd = connectTo("127.0.0.1", port);
    std::printf("matchbox_client: connected to 127.0.0.1:%d\n", port);

    sendOrder(fd, 0, 0, 10000, 50);   // BUY  GTC @ 10000 x50   -> accepted
    sendOrder(fd, 1, 0, 10100, 25);   // SELL GTC @ 10100 x25   -> accepted
    sendOrder(fd, 0, 1, 10050, 10);   // BUY  IOC @ 10050 x10   -> accepted
    sendOrder(fd, 1, 2, 10100, 10);   // SELL FOK @ 10100 x10   -> accepted
    sendOrder(fd, 9, 0, 10000, 10);   // side=9 is invalid       -> rejected
    sendOrder(fd, 0, 0, 0, 10);       // price=0 is invalid      -> rejected

    ::close(fd);
    std::printf("matchbox_client: done\n");
    return 0;
}
