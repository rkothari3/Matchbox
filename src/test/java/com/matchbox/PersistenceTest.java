package com.matchbox;

import com.matchbox.book.*;
import com.matchbox.core.*;
import com.matchbox.io.*;
import org.junit.jupiter.api.*;
import org.junit.jupiter.api.io.TempDir;

import java.io.IOException;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;

import static org.junit.jupiter.api.Assertions.*;

class PersistenceTest {

    @TempDir
    Path tempDir;

    @Test
    void tradeLoggerCreatesHeaderOnNewFile() throws IOException {
        Path file = tempDir.resolve("trades.csv");
        try (CsvTradeLogger logger = new CsvTradeLogger(file)) {
            logger.appendTrade(new Trade(100, 10, 1000L, 1L, 2L));
        }
        List<String> lines = Files.readAllLines(file);
        assertEquals(2, lines.size());
        assertEquals("timestamp,price,quantity,makerOrderId,takerOrderId", lines.get(0));
        assertTrue(lines.get(1).startsWith("1000,100,10,1,2"));
    }

    @Test
    void tradeLoggerAppendsMultipleTrades() throws IOException {
        Path file = tempDir.resolve("trades.csv");
        try (CsvTradeLogger logger = new CsvTradeLogger(file)) {
            logger.appendTrade(new Trade(100, 5, 1000L, 1L, 2L));
            logger.appendTrade(new Trade(101, 3, 2000L, 3L, 4L));
        }
        List<String> lines = Files.readAllLines(file);
        assertEquals(3, lines.size());
        assertTrue(lines.get(1).contains(",100,5,1,2"));
        assertTrue(lines.get(2).contains(",101,3,3,4"));
    }

    @Test
    void tradeLoggerReusesExistingFileWithoutDuplicateHeader() throws IOException {
        Path file = tempDir.resolve("trades.csv");
        try (CsvTradeLogger logger = new CsvTradeLogger(file)) {
            logger.appendTrade(new Trade(100, 5, 1000L, 1L, 2L));
        }
        try (CsvTradeLogger logger2 = new CsvTradeLogger(file)) {
            logger2.appendTrade(new Trade(101, 3, 2000L, 3L, 4L));
        }
        List<String> lines = Files.readAllLines(file);
        assertEquals(3, lines.size());
        assertEquals("timestamp,price,quantity,makerOrderId,takerOrderId", lines.get(0));
    }

    @Test
    void snapshotWriterCreatesHeaderOnNewFile() throws IOException {
        Path file = tempDir.resolve("snapshots.csv");
        try (BookSnapshotWriter writer = new BookSnapshotWriter(file)) {
            writer.writeSnapshot(1000L, TopOfBook.empty(), List.of(), List.of());
        }
        List<String> lines = Files.readAllLines(file);
        assertEquals(3, lines.size());
        assertEquals("side,price,quantity,orderCount", lines.get(0));
        assertTrue(lines.get(1).startsWith("# snapshot_time=1000"));
    }

    @Test
    void snapshotWriterRecordsBidsAndAsks() throws IOException {
        Path file = tempDir.resolve("snapshots.csv");
        try (BookSnapshotWriter writer = new BookSnapshotWriter(file)) {
            writer.writeSnapshot(1000L,
                    new TopOfBook(100, 101, 20, 15),
                    List.of(new BookDepthEntry(100, 20, 2)),
                    List.of(new BookDepthEntry(101, 15, 1)));
        }
        List<String> lines = Files.readAllLines(file);
        assertEquals("side,price,quantity,orderCount", lines.get(0));
        assertTrue(lines.get(1).contains("bestBid=100,bestAsk=101"));
        assertTrue(lines.get(2).contains("B,100,20,2"));
        assertTrue(lines.get(3).contains("A,101,15,1"));
    }

    @Test
    void endToEndPersistenceWorkflow() throws IOException {
        Path tradeFile = tempDir.resolve("trades.csv");
        Path snapFile = tempDir.resolve("snapshots.csv");
        OrderBook book = new OrderBook();

        book.processOrder(OrderCommand.newLimit(Side.SELL, 100, 10));
        book.processOrder(OrderCommand.newLimit(Side.SELL, 101, 5));
        book.processOrder(OrderCommand.newLimit(Side.BUY, 100, 8));

        try (CsvTradeLogger tradeLog = new CsvTradeLogger(tradeFile);
             BookSnapshotWriter snapWriter = new BookSnapshotWriter(snapFile)) {

            for (Trade trade : book.getTradeLog()) {
                tradeLog.appendTrade(trade);
            }

            snapWriter.writeSnapshot(System.nanoTime(),
                    book.getTopOfBook(),
                    book.getBidDepth(),
                    book.getAskDepth());
        }

        List<String> tradeLines = Files.readAllLines(tradeFile);
        assertEquals(2, tradeLines.size());
        assertEquals("timestamp,price,quantity,makerOrderId,takerOrderId", tradeLines.get(0));
        assertTrue(tradeLines.get(1).contains(",100,8,"));

        List<String> snapLines = Files.readAllLines(snapFile);
        assertTrue(snapLines.get(0).contains("side"));
        assertTrue(snapLines.get(2).contains("A,100,2"));
        assertTrue(snapLines.get(3).contains("A,101,5"));
    }

    @Test
    void topOfBookEmptyEncodedCorrectly() throws IOException {
        Path file = tempDir.resolve("snapshots.csv");
        TopOfBook empty = TopOfBook.empty();
        try (BookSnapshotWriter writer = new BookSnapshotWriter(file)) {
            writer.writeSnapshot(5000L, empty, List.of(), List.of());
        }
        List<String> lines = Files.readAllLines(file);
        String header = lines.get(1);
        assertTrue(header.contains("bestBid=0"));
        assertTrue(header.contains("bestAsk=0"));
        assertTrue(header.contains("spread=0"));
    }
}
