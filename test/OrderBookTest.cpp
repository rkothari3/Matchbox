#include "matchbox/book/OrderBook.hpp"

#include <gtest/gtest.h>

using namespace matchbox;

namespace {

class OrderBookTest : public ::testing::Test {
protected:
    OrderBook book;
};

TEST_F(OrderBookTest, EmptyBookHasNoBidOrAsk) {
    TopOfBook top = book.getTopOfBook();
    EXPECT_FALSE(top.hasBid());
    EXPECT_FALSE(top.hasAsk());
    EXPECT_TRUE(book.getBidDepth().empty());
    EXPECT_TRUE(book.getAskDepth().empty());
    EXPECT_TRUE(book.getTradeLog().empty());
}

TEST_F(OrderBookTest, AddSingleLimitBidShowsInTopOfBook) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    TopOfBook top = book.getTopOfBook();
    EXPECT_TRUE(top.hasBid());
    EXPECT_EQ(100, top.bestBid);
    EXPECT_EQ(10, top.bidQuantity);
    EXPECT_FALSE(top.hasAsk());
}

TEST_F(OrderBookTest, AddSingleLimitAskShowsInTopOfBook) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 200, 20));
    TopOfBook top = book.getTopOfBook();
    EXPECT_TRUE(top.hasAsk());
    EXPECT_EQ(200, top.bestAsk);
    EXPECT_EQ(20, top.askQuantity);
    EXPECT_FALSE(top.hasBid());
}

TEST_F(OrderBookTest, BestBidIsHighestPrice) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 101, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 99, 3));
    EXPECT_EQ(101, book.getTopOfBook().bestBid);
}

TEST_F(OrderBookTest, BestAskIsLowestPrice) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 200, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 199, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 201, 3));
    EXPECT_EQ(199, book.getTopOfBook().bestAsk);
}

TEST_F(OrderBookTest, LimitBuyCrossesSpreadMatchesAgainstAsk) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    TopOfBook top = book.getTopOfBook();
    EXPECT_FALSE(top.hasBid());
    EXPECT_FALSE(top.hasAsk());
    ASSERT_EQ(1u, book.getTradeLog().size());
    const Trade& trade = book.getTradeLog()[0];
    EXPECT_EQ(100, trade.price);
    EXPECT_EQ(10, trade.quantity);
}

TEST_F(OrderBookTest, LimitSellCrossesSpreadMatchesAgainstBid) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    TopOfBook top = book.getTopOfBook();
    EXPECT_FALSE(top.hasBid());
    EXPECT_FALSE(top.hasAsk());
    EXPECT_EQ(1u, book.getTradeLog().size());
}

TEST_F(OrderBookTest, PartialFillAgainstMultipleRestingOrdersSamePrice) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 15));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 20));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestAsk);
    EXPECT_EQ(5, top.askQuantity);
    ASSERT_EQ(2u, book.getTradeLog().size());
    EXPECT_EQ(10, book.getTradeLog()[0].quantity);
    EXPECT_EQ(10, book.getTradeLog()[1].quantity);
}

TEST_F(OrderBookTest, PartialFillAcrossMultiplePriceLevels) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 102, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 102, 12));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(102, top.bestAsk);
    EXPECT_EQ(3, top.askQuantity);
    EXPECT_EQ(3u, book.getTradeLog().size());
}

TEST_F(OrderBookTest, MarketBuySweepsAllAsks) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 8));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(101, top.bestAsk);
    EXPECT_EQ(2, top.askQuantity);
    EXPECT_EQ(2u, book.getTradeLog().size());
}

TEST_F(OrderBookTest, MarketSellSweepsAllBids) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 99, 5));
    book.processOrder(OrderCommand::newMarket(Side::SELL, 8));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(99, top.bestBid);
    EXPECT_EQ(2, top.bidQuantity);
    EXPECT_EQ(2u, book.getTradeLog().size());
}

TEST_F(OrderBookTest, PriceTimePriorityRespectedAtSamePrice) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newMarket(Side::SELL, 15));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestBid);
    EXPECT_EQ(15, top.bidQuantity);
    ASSERT_EQ(2u, book.getTradeLog().size());
    EXPECT_EQ(10, book.getTradeLog()[0].quantity);
    EXPECT_EQ(5, book.getTradeLog()[1].quantity);
}

TEST_F(OrderBookTest, CancelRemovesOrder) {
    OrderCommand cmd = OrderCommand::newLimit(Side::BUY, 100, 10);
    book.processOrder(cmd);
    EXPECT_EQ(1, book.orderCount());
    book.processOrder(OrderCommand::newCancel(cmd.order()->orderId()));
    EXPECT_EQ(0, book.orderCount());
    EXPECT_FALSE(book.getTopOfBook().hasBid());
}

TEST_F(OrderBookTest, CancelNonExistentOrderDoesNothing) {
    book.processOrder(OrderCommand::newCancel(999));
    TopOfBook top = book.getTopOfBook();
    EXPECT_FALSE(top.hasBid());
    EXPECT_FALSE(top.hasAsk());
}

TEST_F(OrderBookTest, LimitBuyBecomesNewBestBid) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 101, 5));
    EXPECT_EQ(101, book.getTopOfBook().bestBid);
}

TEST_F(OrderBookTest, LimitSellBecomesNewBestAsk) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 200, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 199, 5));
    EXPECT_EQ(199, book.getTopOfBook().bestAsk);
}

TEST_F(OrderBookTest, DepthSnapshotShowsAllLevels) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 99, 20));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 98, 30));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 15));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 102, 25));

    std::vector<BookDepthEntry> bidDepth = book.getBidDepth();
    ASSERT_EQ(3u, bidDepth.size());
    EXPECT_EQ(100, bidDepth[0].price);
    EXPECT_EQ(10, bidDepth[0].quantity);
    EXPECT_EQ(99, bidDepth[1].price);
    EXPECT_EQ(98, bidDepth[2].price);

    std::vector<BookDepthEntry> askDepth = book.getAskDepth();
    ASSERT_EQ(2u, askDepth.size());
    EXPECT_EQ(101, askDepth[0].price);
    EXPECT_EQ(102, askDepth[1].price);
}

TEST_F(OrderBookTest, MarketBuyWithInsufficientLiquidity) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 10));
    TopOfBook top = book.getTopOfBook();
    ASSERT_EQ(1u, book.getTradeLog().size());
    EXPECT_EQ(5, book.getTradeLog()[0].quantity);
    EXPECT_FALSE(top.hasAsk());
    EXPECT_FALSE(top.hasBid());
}

TEST_F(OrderBookTest, ClearTradeLog) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 5));
    book.processOrder(OrderCommand::newMarket(Side::BUY, 5));
    EXPECT_EQ(1u, book.getTradeLog().size());
    book.clearTradeLog();
    EXPECT_TRUE(book.getTradeLog().empty());
}

TEST_F(OrderBookTest, SpreadComputedCorrectly) {
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 10));
    EXPECT_EQ(1, book.getTopOfBook().spread());
}

TEST_F(OrderBookTest, PartialFillMakerPartiallyFilledRestingOrderStays) {
    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 6));
    TopOfBook top = book.getTopOfBook();
    EXPECT_EQ(100, top.bestAsk);
    EXPECT_EQ(4, top.askQuantity);
    EXPECT_EQ(1, book.orderCount());
}

}  // namespace
