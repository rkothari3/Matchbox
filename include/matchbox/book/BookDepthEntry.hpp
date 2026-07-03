#pragma once

namespace matchbox {

// One rung of the depth ladder: aggregate quantity and order count at a price.
struct BookDepthEntry {
    long price;
    long quantity;
    int orderCount;
};

}
