#include "matchbox/book/OrderBook.hpp"

#include <algorithm>

#include "matchbox/core/Clock.hpp"

namespace matchbox {

OrderBook::OrderBook()
    : bids_(PriceComparator{true}),   // descending: best bid = highest price
      asks_(PriceComparator{false}) {} // ascending: best ask = lowest price

void OrderBook::processOrder(const OrderCommand& cmd) {
    switch (cmd.type()) {
        case OrderType::LIMIT:
            processLimit(cmd.order());
            break;
        case OrderType::MARKET:
            processMarket(cmd.order());
            break;
        case OrderType::CANCEL:
            processCancel(cmd.cancelOrderId());
            break;
        case OrderType::MODIFY:
            // Introduced in Stage 2 (order lifecycle).
            break;
    }
}

void OrderBook::processLimit(const std::shared_ptr<Order>& taker) {
    if (taker->side() == Side::BUY) {
        matchAgainstBook(*taker, asks_, true);
    } else {
        matchAgainstBook(*taker, bids_, true);
    }
    if (!taker->isFilled()) {
        addToBook(taker);
    }
}

void OrderBook::processMarket(const std::shared_ptr<Order>& taker) {
    if (taker->side() == Side::BUY) {
        matchAgainstBook(*taker, asks_, false);
    } else {
        matchAgainstBook(*taker, bids_, false);
    }
    // Market orders never rest: any unfilled remainder is simply dropped.
}

void OrderBook::matchAgainstBook(Order& taker, Book& book, bool isLimit) {
    while (!taker.isFilled() && !book.empty()) {
        auto best = book.begin();  // comparator guarantees this is the best price
        long bestPrice = best->first;
        if (isLimit) {
            // Buy stops once the best ask is above its limit; sell once the
            // best bid is below its limit.
            if (taker.side() == Side::BUY && taker.price() < bestPrice) break;
            if (taker.side() == Side::SELL && taker.price() > bestPrice) break;
        }
        matchAgainstLevel(taker, best->second);
        if (best->second.isEmpty()) book.erase(best);
    }
}

void OrderBook::matchAgainstLevel(Order& taker, PriceLevel& level) {
    while (!taker.isFilled() && !level.isEmpty()) {
        std::shared_ptr<Order> resting = level.peek();
        long matchQty = std::min(taker.quantity(), resting->quantity());
        long ts = nowNanos();

        tradeLog_.push_back(Trade{level.price(), matchQty, ts,
                                  resting->orderId(), taker.orderId()});

        resting->reduceQuantity(matchQty);
        taker.reduceQuantity(matchQty);

        if (resting->isFilled()) {
            level.poll();
            orderMap_.erase(resting->orderId());
        }
    }
}

void OrderBook::addToBook(const std::shared_ptr<Order>& order) {
    Book& book = order->side() == Side::BUY ? bids_ : asks_;
    auto it = book.find(order->price());
    if (it == book.end()) {
        it = book.emplace(order->price(), PriceLevel(order->price())).first;
    }
    it->second.add(order);
    orderMap_[order->orderId()] = order;
}

void OrderBook::processCancel(long orderId) {
    auto it = orderMap_.find(orderId);
    if (it == orderMap_.end()) return;

    std::shared_ptr<Order> order = it->second;
    orderMap_.erase(it);

    Book& book = order->side() == Side::BUY ? bids_ : asks_;
    auto levelIt = book.find(order->price());
    if (levelIt != book.end()) {
        levelIt->second.remove(order);
        if (levelIt->second.isEmpty()) book.erase(levelIt);
    }
}

TopOfBook OrderBook::getTopOfBook() const {
    long bidPrice = std::numeric_limits<long>::min(), bidQty = 0;
    long askPrice = std::numeric_limits<long>::max(), askQty = 0;

    if (!bids_.empty()) {
        const auto& e = *bids_.begin();
        bidPrice = e.first;
        bidQty = e.second.totalQuantity();
    }
    if (!asks_.empty()) {
        const auto& e = *asks_.begin();
        askPrice = e.first;
        askQty = e.second.totalQuantity();
    }
    return TopOfBook{bidPrice, askPrice, bidQty, askQty};
}

std::vector<BookDepthEntry> OrderBook::getBidDepth() const {
    std::vector<BookDepthEntry> result;
    result.reserve(bids_.size());
    for (const auto& [price, level] : bids_) {
        result.push_back(BookDepthEntry{price, level.totalQuantity(), level.size()});
    }
    return result;
}

std::vector<BookDepthEntry> OrderBook::getAskDepth() const {
    std::vector<BookDepthEntry> result;
    result.reserve(asks_.size());
    for (const auto& [price, level] : asks_) {
        result.push_back(BookDepthEntry{price, level.totalQuantity(), level.size()});
    }
    return result;
}

}
