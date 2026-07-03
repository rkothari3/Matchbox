#pragma once

namespace matchbox {

// The four instruction types the matching engine understands.
enum class OrderType {
    LIMIT,   // rest at a fixed price (marketable portion crosses first)
    MARKET,  // take best available price, never rests
    CANCEL,  // remove a resting order by id
    MODIFY   // cancel-replace: change price and/or quantity of a resting order
};

}
