#pragma once

#include <limits>

namespace matchbox {

// Snapshot of the best bid/ask and their aggregate quantities.
// Empty sides are encoded with sentinel prices (min bid / max ask) so that
// spread arithmetic and presence checks stay branch-simple.
struct TopOfBook {
    long bestBid;
    long bestAsk;
    long bidQuantity;
    long askQuantity;

    static TopOfBook empty() {
        return TopOfBook{std::numeric_limits<long>::min(),
                         std::numeric_limits<long>::max(), 0, 0};
    }

    bool hasBid() const { return bestBid != std::numeric_limits<long>::min(); }
    bool hasAsk() const { return bestAsk != std::numeric_limits<long>::max(); }
    bool hasBoth() const { return hasBid() && hasAsk(); }
    long spread() const {
        return hasBoth() ? bestAsk - bestBid : std::numeric_limits<long>::max();
    }
};

}
