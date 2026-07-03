#pragma once

#include <chrono>

namespace matchbox {

// Monotonic nanosecond timestamp. steady_clock never goes backwards, which is
// what we want for ordering events and stamping trades.
inline long nowNanos() {
    return std::chrono::duration_cast<std::chrono::nanoseconds>(
               std::chrono::steady_clock::now().time_since_epoch())
        .count();
}

}
