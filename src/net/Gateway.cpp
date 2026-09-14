#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <optional>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include "matchbox/book/OrderBook.hpp"
#include "matchbox/core/OrderCommand.hpp"
#include "matchbox/net/SocketUtil.hpp"
#include "matchbox/net/WireProtocol.hpp"

using namespace matchbox;
using namespace matchbox::net;

namespace {

// Hardcoded reject reasons (used only for the server's log line, so you can
// see WHY an order was refused; the wire ack just carries success=0):
//   1 = bad side, 2 = bad time-in-force, 3 = bad price, 4 = bad quantity
std::optional<OrderCommand> buildCommand(const NewOrderMessage& msg, uint8_t& reason) {
    if (msg.side > 1) { reason = 1; return std::nullopt; }
    if (msg.timeInForce > 3) { reason = 2; return std::nullopt; }
    if (msg.price <= 0) { reason = 3; return std::nullopt; }
    if (msg.quantity <= 0) { reason = 4; return std::nullopt; }
    return OrderCommand::newLimit(
        static_cast<Side>(msg.side), msg.price, msg.quantity,
        static_cast<TimeInForce>(msg.timeInForce));
}

void sendAck(int fd, uint8_t success, long orderId) {
    NewOrderAck ack{static_cast<uint8_t>(MessageType::NEW_ORDER_ACK), success, orderId};
    if (sendExact(fd, &ack, sizeof(ack)) < 0) std::perror("send ack");
}

[[noreturn]] void die(const char* msg) {
    std::perror(msg);
    std::exit(1);
}

}  // namespace

int main(int argc, char** argv) {
    int port = argc > 1 ? std::atoi(argv[1]) : 9100;

    // If the client drops the connection while we are replying, send() would
    // normally kill this process with SIGPIPE. We want send() to just fail
    // instead, so the loop can notice and exit cleanly.
    std::signal(SIGPIPE, SIG_IGN);

    int fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) die("socket");

    // Allow rebinding the same port right away after a restart (otherwise a
    // socket in TIME_WAIT can make bind() fail for a while).
    int one = 1;
    ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof(one));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);  // accept connections on any NIC
    addr.sin_port = htons(static_cast<uint16_t>(port));
    if (::bind(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) die("bind");
    if (::listen(fd, 1) < 0) die("listen");

    std::printf("matchbox_gateway: listening on 0.0.0.0:%d\n", port);
    std::fflush(stdout);

    // Accept exactly ONE client, then serve it until it disconnects.
    sockaddr_in peer{};
    socklen_t peerLen = sizeof(peer);
    int client = ::accept(fd, reinterpret_cast<sockaddr*>(&peer), &peerLen);
    if (client < 0) die("accept");

    char ip[INET_ADDRSTRLEN];
    std::printf("matchbox_gateway: client connected from %s\n",
                inet_ntop(AF_INET, &peer.sin_addr, ip, sizeof(ip)));
    std::fflush(stdout);

    OrderBook book;
    long served = 0;

    for (;;) {
        NewOrderMessage msg;
        int n = recvExact(client, &msg, sizeof(msg));
        if (n <= 0) break;  // 0 = client closed, -1 = socket error

        if (msg.type != static_cast<uint8_t>(MessageType::NEW_ORDER)) {
            sendAck(client, 0, 0);
            continue;
        }

        uint8_t reason = 0;
        auto cmdOpt = buildCommand(msg, reason);
        if (!cmdOpt) {
            std::printf("matchbox_gateway: rejected order (reason %u)\n", reason);
            std::fflush(stdout);
            sendAck(client, 0, 0);
            continue;
        }
        OrderCommand cmd = std::move(*cmdOpt);

        long orderId = cmd.order()->orderId();  // assigned at construction
        book.processOrder(cmd);                  // the one engine entry point
        ++served;

        sendAck(client, 1, orderId);
    }

    ::close(client);
    ::close(fd);
    std::printf("matchbox_gateway: client disconnected, %ld orders processed\n", served);
    return 0;
}
