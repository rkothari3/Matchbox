#pragma once

namespace matchbox {

// Controls how long an order lives and how it fills.
enum class TimeInForce {
    GTC,  // Good-Till-Cancel: rests until filled or cancelled
    IOC,  // Immediate-Or-Cancel: fill what crosses now, cancel the rest
    FOK,  // Fill-Or-Kill: fill in full immediately, else reject entirely
    GTD   // Good-Till-Date: rests until an expiry timestamp
};

}
