package com.matchbox;

import com.matchbox.book.*;
import com.matchbox.core.*;
import org.junit.jupiter.api.*;

import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

class OrderBookTest {

    private OrderBook book;

    @BeforeEach
    void setUp() {
        book = new OrderBook();
    }

    @Test
    void emptyBookHasNoBidOrAsk() {
        TopOfBook top = book.getTopOfBook();
        assertFalse(top.hasBid());
        assertFalse(top.hasAsk());
        assertTrue(book.getBidDepth().isEmpty());
        assertTrue(book.getAskDepth().isEmpty());
        assertTrue(book.getTradeLog().isEmpty());
    }

    @Test
    void addSingleLimitBidShowsInTopOfBook() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        TopOfBook top = book.getTopOfBook();
        assertTrue(top.hasBid());
        assertEquals(100, top.bestBid());
        assertEquals(10, top.bidQuantity());
        assertFalse(top.hasAsk());
    }

    @Test
    void addSingleLimitAskShowsInTopOfBook() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 200, 20));
        TopOfBook top = book.getTopOfBook();
        assertTrue(top.hasAsk());
        assertEquals(200, top.bestAsk());
        assertEquals(20, top.askQuantity());
        assertFalse(top.hasBid());
    }

    @Test
    void bestBidIsHighestPrice() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 101, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 99, 3));
        assertEquals(101, book.getTopOfBook().bestBid());
    }

    @Test
    void bestAskIsLowestPrice() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 200, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 199, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 201, 3));
        assertEquals(199, book.getTopOfBook().bestAsk());
    }

    @Test
    void limitBuyCrossesSpreadMatchesAgainstAsk() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        TopOfBook top = book.getTopOfBook();
        assertFalse(top.hasBid());
        assertFalse(top.hasAsk());
        assertEquals(1, book.getTradeLog().size());
        Trade trade = book.getTradeLog().get(0);
        assertEquals(100, trade.getPrice());
        assertEquals(10, trade.getQuantity());
    }

    @Test
    void limitSellCrossesSpreadMatchesAgainstBid() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        TopOfBook top = book.getTopOfBook();
        assertFalse(top.hasBid());
        assertFalse(top.hasAsk());
        assertEquals(1, book.getTradeLog().size());
    }

    @Test
    void partialFillAgainstMultipleRestingOrdersSamePrice() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 15));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 20));
        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestAsk());
        assertEquals(5, top.askQuantity());
        assertEquals(2, book.getTradeLog().size());
        assertEquals(10, book.getTradeLog().get(0).getQuantity());
        assertEquals(10, book.getTradeLog().get(1).getQuantity());
    }

    @Test
    void partialFillAcrossMultiplePriceLevels() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 102, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 102, 12));
        TopOfBook top = book.getTopOfBook();
        assertEquals(102, top.bestAsk());
        assertEquals(3, top.askQuantity());
        assertEquals(3, book.getTradeLog().size());
    }

    @Test
    void marketBuySweepsAllAsks() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 8));
        TopOfBook top = book.getTopOfBook();
        assertEquals(101, top.bestAsk());
        assertEquals(2, top.askQuantity());
        assertEquals(2, book.getTradeLog().size());
    }

    @Test
    void marketSellSweepsAllBids() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 99, 5));
        book.processOrder(OrderCommand.newMarket(Side.SELL, 8));
        TopOfBook top = book.getTopOfBook();
        assertEquals(99, top.bestBid());
        assertEquals(2, top.bidQuantity());
        assertEquals(2, book.getTradeLog().size());
    }

    @Test
    void priceTimePriorityRespectedAtSamePrice() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newMarket(Side.SELL, 15));
        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestBid());
        assertEquals(15, top.bidQuantity());
        assertEquals(2, book.getTradeLog().size());
        assertEquals(10, book.getTradeLog().get(0).getQuantity());
        assertEquals(5, book.getTradeLog().get(1).getQuantity());
    }

    @Test
    void cancelRemovesOrder() {
        OrderCommand cmd = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd);
        assertEquals(1, book.orderCount());
        book.processOrder(OrderCommand.newCancel(cmd.getOrder().getOrderId()));
        assertEquals(0, book.orderCount());
        assertFalse(book.getTopOfBook().hasBid());
    }

    @Test
    void cancelNonExistentOrderDoesNothing() {
        book.processOrder(OrderCommand.newCancel(999));
        TopOfBook top = book.getTopOfBook();
        assertFalse(top.hasBid());
        assertFalse(top.hasAsk());
    }

    @Test
    void modifyOrderQuantity() {
        OrderCommand cmd = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd);
        long orderId = cmd.getOrder().getOrderId();
        book.processOrder(OrderCommand.newModify(orderId, 100, 20));
        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestBid());
        assertEquals(20, top.bidQuantity());
        assertEquals(1, book.orderCount());
    }

    @Test
    void modifyOrderPriceChangesLevel() {
        OrderCommand cmd = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd);
        book.processOrder(OrderCommand.newLimit(Side.BUY, 99, 5));
        long orderId = cmd.getOrder().getOrderId();
        book.processOrder(OrderCommand.newModify(orderId, 99, 10));
        TopOfBook top = book.getTopOfBook();
        assertEquals(99, top.bestBid());
        assertEquals(15, top.bidQuantity());
    }

    @Test
    void modifyToCrossPriceTriggersMatch() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        OrderCommand cmd = OrderCommand.newLimit(Side.BUY, 90, 10);
        book.processOrder(cmd);
        book.processOrder(OrderCommand.newModify(cmd.getOrder().getOrderId(), 100, 10));
        assertFalse(book.getTopOfBook().hasBid());
        assertFalse(book.getTopOfBook().hasAsk());
        assertEquals(1, book.getTradeLog().size());
    }

    @Test
    void limitBuyBecomesNewBestBid() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 101, 5));
        assertEquals(101, book.getTopOfBook().bestBid());
    }

    @Test
    void limitSellBecomesNewBestAsk() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 200, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 199, 5));
        assertEquals(199, book.getTopOfBook().bestAsk());
    }

    @Test
    void depthSnapshotShowsAllLevels() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 99, 20));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 98, 30));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 15));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 102, 25));

        List<BookDepthEntry> bidDepth = book.getBidDepth();
        assertEquals(3, bidDepth.size());
        assertEquals(100, bidDepth.get(0).price());
        assertEquals(10, bidDepth.get(0).quantity());
        assertEquals(99, bidDepth.get(1).price());
        assertEquals(98, bidDepth.get(2).price());

        List<BookDepthEntry> askDepth = book.getAskDepth();
        assertEquals(2, askDepth.size());
        assertEquals(101, askDepth.get(0).price());
        assertEquals(102, askDepth.get(1).price());
    }

    @Test
    void marketBuyWithInsufficientLiquidity() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 10));
        TopOfBook top = book.getTopOfBook();
        assertEquals(5, book.getTradeLog().get(0).getQuantity());
        assertFalse(top.hasAsk());
        assertFalse(top.hasBid());
    }

    @Test
    void clearTradeLog() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 5));
        assertEquals(1, book.getTradeLog().size());
        book.clearTradeLog();
        assertTrue(book.getTradeLog().isEmpty());
    }

    @Test
    void spreadComputedCorrectly() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 10));
        assertEquals(1, book.getTopOfBook().spread());
    }

    @Test
    void partialFillMakerPartiallyFilledRestingOrderStays() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 6));
        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestAsk());
        assertEquals(4, top.askQuantity());
        assertEquals(1, book.orderCount());
    }

    // --- Stage 2: Modify-in-place preserves time priority ---

    @Test
    void modifySamePriceKeepsTimePriority() {
        OrderCommand cmd1 = OrderCommand.newLimit(Side.BUY, 100, 10);
        OrderCommand cmd2 = OrderCommand.newLimit(Side.BUY, 100, 10);
        OrderCommand cmd3 = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd1);
        book.processOrder(cmd2);
        book.processOrder(cmd3);
        long firstId = cmd1.getOrder().getOrderId();

        book.processOrder(OrderCommand.newModify(firstId, 100, 20));

        book.processOrder(OrderCommand.newMarket(Side.SELL, 25));
        assertEquals(2, book.getTradeLog().size());
        Trade t1 = book.getTradeLog().get(0);
        Trade t2 = book.getTradeLog().get(1);
        assertEquals(20, t1.getQuantity());
        assertEquals(5, t2.getQuantity());
        assertEquals(firstId, t1.getMakerOrderId());
        assertEquals(cmd2.getOrder().getOrderId(), t2.getMakerOrderId());
    }

    @Test
    void modifyDifferentPriceLosesTimePriority() {
        OrderCommand cmd1 = OrderCommand.newLimit(Side.BUY, 99, 10);
        OrderCommand cmd2 = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd1);
        book.processOrder(cmd2);

        book.processOrder(OrderCommand.newModify(cmd1.getOrder().getOrderId(), 100, 10));

        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestBid());
        assertEquals(20, top.bidQuantity());

        book.processOrder(OrderCommand.newMarket(Side.SELL, 10));
        Trade t = book.getTradeLog().get(0);
        assertEquals(cmd2.getOrder().getOrderId(), t.getMakerOrderId());
    }

    // --- Stage 2: IOC (Immediate-Or-Cancel) ---

    @Test
    void iocLimitPartialFillCancelsRemainder() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.IOC, 0));
        TopOfBook top = book.getTopOfBook();
        assertEquals(1, book.getTradeLog().size());
        assertEquals(5, book.getTradeLog().get(0).getQuantity());
        assertFalse(top.hasBid());
    }

    @Test
    void iocLimitNoCrossDoesNotRest() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 90, 10,
                TimeInForce.IOC, 0));
        TopOfBook top = book.getTopOfBook();
        assertFalse(top.hasBid());
        assertTrue(book.getTradeLog().isEmpty());
    }

    @Test
    void iocMarketWorksAsRegularMarket() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 3,
                TimeInForce.IOC, 0));
        assertEquals(1, book.getTradeLog().size());
        assertEquals(3, book.getTradeLog().get(0).getQuantity());
        TopOfBook top = book.getTopOfBook();
        assertEquals(100, top.bestAsk());
        assertEquals(2, top.askQuantity());
    }

    // --- Stage 2: FOK (Fill-Or-Kill) ---

    @Test
    void fokLimitInsufficientLiquidityNoFill() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.FOK, 0));
        assertTrue(book.getTradeLog().isEmpty());
        assertEquals(1, book.orderCount());
    }

    @Test
    void fokLimitSufficientLiquidityFills() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 101, 10,
                TimeInForce.FOK, 0));
        assertEquals(2, book.getTradeLog().size());
        assertEquals(5, book.getTradeLog().get(0).getQuantity());
        assertEquals(5, book.getTradeLog().get(1).getQuantity());
        assertFalse(book.getTopOfBook().hasBid());
        assertFalse(book.getTopOfBook().hasAsk());
    }

    @Test
    void fokLimitPartialPriceDepthFillsFully() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.FOK, 0));
        assertTrue(book.getTradeLog().isEmpty());
        assertEquals(2, book.orderCount());
    }

    @Test
    void fokMarketInsufficientLiquidityNoFill() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 10,
                TimeInForce.FOK, 0));
        assertTrue(book.getTradeLog().isEmpty());
        assertEquals(1, book.orderCount());
    }

    @Test
    void fokMarketSufficientLiquidityFills() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 5));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newMarket(Side.BUY, 10,
                TimeInForce.FOK, 0));
        assertEquals(2, book.getTradeLog().size());
        assertFalse(book.getTopOfBook().hasBid());
        assertFalse(book.getTopOfBook().hasAsk());
    }

    // --- Stage 2: GTD (Good-Till-Date) expiry ---

    @Test
    void gtdOrderExpires() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.GTD, 1000L));
        assertEquals(1, book.orderCount());
        int expired = book.expireOrders(1001L);
        assertEquals(1, expired);
        assertEquals(0, book.orderCount());
        assertFalse(book.getTopOfBook().hasBid());
    }

    @Test
    void gtdOrderNotExpiredBeforeExpiry() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.GTD, 1000L));
        int expired = book.expireOrders(999L);
        assertEquals(0, expired);
        assertEquals(1, book.orderCount());
    }

    @Test
    void gtdOrderExpiresAtExactTime() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.GTD, 1000L));
        int expired = book.expireOrders(1000L);
        assertEquals(1, expired);
    }

    @Test
    void gtcOrderNeverExpires() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.GTC, 0));
        int expired = book.expireOrders(Long.MAX_VALUE);
        assertEquals(0, expired);
        assertEquals(1, book.orderCount());
    }

    @Test
    void gtdMultipleOrdersExpireSelectively() {
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 10,
                TimeInForce.GTD, 500L));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 99, 10,
                TimeInForce.GTD, 1500L));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 98, 10,
                TimeInForce.GTC, 0));
        assertEquals(3, book.orderCount());

        int expired = book.expireOrders(1000L);
        assertEquals(1, expired);
        assertEquals(2, book.orderCount());
        assertEquals(99, book.getTopOfBook().bestBid());
    }

    @Test
    void gtdOrderExpiresAfterPartialFill() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        OrderCommand buy = OrderCommand.newLimit(Side.BUY, 100, 20,
                TimeInForce.GTD, 1000L);
        book.processOrder(buy);
        assertEquals(1, book.getTradeLog().size());
        assertEquals(1, book.orderCount());

        int expired = book.expireOrders(1001L);
        assertEquals(1, expired);
        assertEquals(0, book.orderCount());
    }

    @Test
    void modifyOrderTracksFilledQuantity() {
        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        OrderCommand cmd = OrderCommand.newLimit(Side.BUY, 100, 10);
        book.processOrder(cmd);
        assertEquals(10, cmd.getOrder().getFilledQuantity());
        assertTrue(cmd.getOrder().isFilled());
    }
}
