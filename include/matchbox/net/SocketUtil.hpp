#pragma once

#include <cerrno>
#include <cstddef>

#include <sys/socket.h>

namespace matchbox::net {

// TCP is a byte stream, not a message stream: a single recv()/send() call can
// move fewer bytes than you asked for (and the split point has nothing to do
// with your message boundaries). These helpers loop until exactly n bytes have
// been moved, which is what makes fixed-size messages safe to read/write.
//
// Return values (identical for both):
//   n  on success (all n bytes transferred),
//   0  if the peer closed the connection before n bytes arrived (recv only),
//  -1  on socket error.
inline int recvExact(int fd, void* buf, std::size_t n) {
    char* p = static_cast<char*>(buf);
    std::size_t got = 0;
    while (got < n) {
        ssize_t r = ::recv(fd, p + got, n - got, 0);
        if (r == 0) return 0;      // peer closed the connection
        if (r < 0) {
            if (errno == EINTR) continue;  // interrupted by a signal: retry
            return -1;                     // real socket error
        }
        got += static_cast<std::size_t>(r);
    }
    return static_cast<int>(n);
}

inline int sendExact(int fd, const void* buf, std::size_t n) {
    const char* p = static_cast<const char*>(buf);
    std::size_t sent = 0;
    while (sent < n) {
        ssize_t s = ::send(fd, p + sent, n - sent, 0);
        if (s < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (s == 0) return -1;     // send never returns 0 normally; treat as error
        sent += static_cast<std::size_t>(s);
    }
    return static_cast<int>(n);
}

}  // namespace matchbox::net
