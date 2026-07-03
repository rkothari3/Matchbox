#include "matchbox/book/OrderBook.hpp"

#include <climits>

#include <gtest/gtest.h>

using namespace matchbox;

namespace {

class OrderLifecycleTest : public ::testing::Test {
protected:
    OrderBook book;
};

// --- Modify (cancel-replace) ---

TEST_F(OrderLifecycleTest, ModifyOrderQuantity) {
    OrderCommand cmd = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd);
    long orderId = cmd.order()->orderId();
    book.processOrder(OrderCommand::newModify(orderId, 100, 20));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestBid);
    EXPECT_EQ(20, top.bidQuantity);
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderLifecycleTest, ModifyOrderPriceChangesLevel) {
    OrderCommand cmd = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd);
    book.processOrder(OrderCommand::newLimit(Side::BUY, 99, 5));
    book.processOrder(OrderCommand::newModify(cmd.order()->orderId(), 99, 10));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(99, top.bestBid);
    EXPECT_EQ(15, top.bidQuantity);
}

TEST_F(OrderLifecycleTest, ModifyToCrossPriceTriggersMatch) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    OrderCommand cmd = OrderCommand::newLimit(Side::BUY, 90, 10);
    book.processOrder(cmd);
    book.processOrder(OrderCommand::newModify(cmd.order()->orderId(), 100, 10));
    EXPECT_FALSE(book.getTopOfBook().hasBid());
    EXPECT_FALSE(book.getTopOfBook().hasAsk());
    EXPECT_EQ(1u, book.getTradeLog().size());
}

TEST_F(OrderLifecycleTest, ModifySamePriceKeepsTimePriority) {
    OrderCommand cmd1 = OrderCommand::newLimit(Side::BUY, 100, 10);
    OrderCommand cmd2 = OrderCommand::newLimit(Side::BUY, 100, 10);
    OrderCommand cmd3 = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd1);
    book.processOrder(cmd2);
    book.processOrder(cmd3);
    long firstId = cmd1.order()->orderId();

    book.processOrder(OrderCommand::newModify(firstId, 100, 20));

    book.processOrder(OrderCommand::newMarket(Side::SELL, 25));
    ASSERT_EQ(2u, book.getTradeLog().size());
    const Trade& t1 = book.getTradeLog()[0];
    const Trade& t2 = book.getTradeLog()[1];
    EXPECT_EQ(20, t1.quantity);
    EXPECT_EQ(5, t2.quantity);
    EXPECT_EQ(firstId, t1.makerOrderId);
    EXPECT_EQ(cmd2.order()->orderId(), t2.makerOrderId);
}

TEST_F(OrderLifecycleTest, ModifyDifferentPriceLosesTimePriority) {
    OrderCommand cmd1 = OrderCommand::newLimit(Side::BUY, 99, 10);
    OrderCommand cmd2 = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd1);
    book.processOrder(cmd2);

    book.processOrder(OrderCommand::newModify(cmd1.order()->orderId(), 100, 10));

    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestBid);
    EXPECT_EQ(20, top.bidQuantity);

    book.processOrder(OrderCommand::newMarket(Side::SELL, 10));
    EXPECT_EQ(cmd2.order()->orderId(), book.getTradeLog()[0].makerOrderId);
}

TEST_F(OrderLifecycleTest, ModifyOrderTracksFilledQuantity) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    OrderCommand cmd = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd);
    EXPECT_EQ(10, cmd.order()->filledQuantity());
    EXPECT_TRUE(cmd.order()->isFilled());
}

// --- IOC (Immediate-Or-Cancel) ---

TEST_F(OrderLifecycleTest, IocLimitPartialFillCancelsRemainder) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::IOC));
    TopOfBook top = book.getTopOfBook();
    ASSERT_EQ(1u, book.getTradeLog().size());
    EXPECT_EQ(5, book.getTradeLog()[0].quantity);
    EXPECT_FALSE(top.hasBid());
}

TEST_F(OrderLifecycleTest, IocLimitNoCrossDoesNotRest) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 90, 10, TimeInForce::IOC));
    TopOfBook top = book.getTopOfBook();
    EXPECT_FALSE(top.hasBid());
    EXPECT_TRUE(book.getTradeLog().empty());
}

TEST_F(OrderLifecycleTest, IocMarketWorksAsRegularMarket) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 3, TimeInForce::IOC));
    ASSERT_EQ(1u, book.getTradeLog().size());
    EXPECT_EQ(3, book.getTradeLog()[0].quantity);
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestAsk);
    EXPECT_EQ(2, top.askQuantity);
}

// --- FOK (Fill-Or-Kill) ---

TEST_F(OrderLifecycleTest, FokLimitInsufficientLiquidityNoFill) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::FOK));
    EXPECT_TRUE(book.getTradeLog().empty());
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderLifecycleTest, FokLimitSufficientLiquidityFills) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 101, 10, TimeInForce::FOK));
    ASSERT_EQ(2u, book.getTradeLog().size());
    EXPECT_EQ(5, book.getTradeLog()[0].quantity);
    EXPECT_EQ(5, book.getTradeLog()[1].quantity);
    EXPECT_FALSE(book.getTopOfBook().hasBid());
    EXPECT_FALSE(book.getTopOfBook().hasAsk());
}

TEST_F(OrderLifecycleTest, FokLimitPartialPriceDepthFillsFully) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::FOK));
    EXPECT_TRUE(book.getTradeLog().empty());
    EXPECT_EQ(2, book.orderCount());
}

TEST_F(OrderLifecycleTest, FokMarketInsufficientLiquidityNoFill) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 10, TimeInForce::FOK));
    EXPECT_TRUE(book.getTradeLog().empty());
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderLifecycleTest, FokMarketSufficientLiquidityFills) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 10, TimeInForce::FOK));
    EXPECT_EQ(2u, book.getTradeLog().size());
    EXPECT_FALSE(book.getTopOfBook().hasBid());
    EXPECT_FALSE(book.getTopOfBook().hasAsk());
}

// --- GTD (Good-Till-Date) expiry ---

TEST_F(OrderLifecycleTest, GtdOrderExpires) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::GTD, 1000L));
    EXPECT_EQ(1, book.orderCount());
    int expired = book.expireOrders(1001L);
    EXPECT_EQ(1, expired);
    EXPECT_EQ(0, book.orderCount());
    EXPECT_FALSE(book.getTopOfBook().hasBid());
}

TEST_F(OrderLifecycleTest, GtdOrderNotExpiredBeforeExpiry) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::GTD, 1000L));
    int expired = book.expireOrders(999L);
    EXPECT_EQ(0, expired);
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderLifecycleTest, GtdOrderExpiresAtExactTime) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::GTD, 1000L));
    int expired = book.expireOrders(1000L);
    EXPECT_EQ(1, expired);
}

TEST_F(OrderLifecycleTest, GtcOrderNeverExpires) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::GTC, 0));
    int expired = book.expireOrders(LONG_MAX);
    EXPECT_EQ(0, expired);
    EXPECT_EQ(1, book.orderCount());
}

TEST_F(OrderLifecycleTest, GtdMultipleOrdersExpireSelectively) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10, TimeInForce::GTD, 500L));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 99, 10, TimeInForce::GTD, 1500L));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 98, 10, TimeInForce::GTC, 0));
    EXPECT_EQ(3, book.orderCount());

    int expired = book.expireOrders(1000L);
    EXPECT_EQ(1, expired);
    EXPECT_EQ(2, book.orderCount());
    EXPECT_EQ(99, book.getTopOfBook().bestBid);
}

TEST_F(OrderLifecycleTest, GtdOrderExpiresAfterPartialFill) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 20, TimeInForce::GTD, 1000L));
    EXPECT_EQ(1u, book.getTradeLog().size());
    EXPECT_EQ(1, book.orderCount());

    int expired = book.expireOrders(1001L);
    EXPECT_EQ(1, expired);
    EXPECT_EQ(0, book.orderCount());
}

}  // namespace
