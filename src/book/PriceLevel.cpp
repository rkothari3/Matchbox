#include "matchbox/book/PriceLevel.hpp"

#include <algorithm>

namespace matchbox {

bool PriceLevel::remove(const std::shared_ptr<Order>& order) {
    auto it = std::find(orders_.begin(), orders_.end(), order);
    if (it == orders_.end()) return false;
    orders_.erase(it);
    return true;
}

long PriceLevel::totalQuantity() const {
    long total = 0;
    for (const auto& order : orders_) total += order->quantity();
    return total;
}

}
