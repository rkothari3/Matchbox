#pragma once

#include <ostream>

namespace matchbox {

// Immutable record of one match. maker = resting order, taker = incoming.
// Position in the trade log is its implicit identity (no separate trade id).
struct Trade {
    long price;
    long quantity;
    long timestamp;
    long makerOrderId;
    long takerOrderId;
};

inline std::ostream& operator<<(std::ostream& os, const Trade& t) {
    return os << "Trade{price=" << t.price << ", qty=" << t.quantity
              << ", ts=" << t.timestamp << ", maker=" << t.makerOrderId
              << ", taker=" << t.takerOrderId << '}';
}

}
