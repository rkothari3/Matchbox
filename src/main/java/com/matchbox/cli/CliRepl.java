package com.matchbox.cli;

import com.matchbox.book.*;
import com.matchbox.core.*;
import java.io.*;
import java.util.*;

/**
 * Interactive CLI for the order book.
 *
 * Usage:
 *   java com.matchbox.cli.CliRepl < orders.txt
 *   java com.matchbox.cli.CliRepl          (interactive stdin)
 *
 * Input format per line:
 *   LIMIT <BUY|SELL> <price> <qty>
 *   MARKET <BUY|SELL> <qty>
 *   CANCEL <orderId>
 *   MODIFY <orderId> <newPrice> <newQty>
 *   # comment
 *   (blank line = print depth snapshot)
 */
public class CliRepl {

    private final OrderBook book;
    private final Scanner scanner;
    private long commandCount;

    public CliRepl(InputStream in) {
        this.book = new OrderBook();
        this.scanner = new Scanner(in);
        this.commandCount = 0;
    }

    public void run() {
        while (scanner.hasNextLine()) {
            String line = scanner.nextLine().trim();
            if (line.isEmpty()) {
                printDepth();
                continue;
            }
            if (line.startsWith("#") || line.startsWith("//")) continue;

            commandCount++;
            try {
                processLine(line);
            } catch (Exception e) {
                System.out.printf("  Error: %s%n", e.getMessage());
            }
        }
    }

    private void processLine(String line) {
        String[] parts = line.split("\\s+");
        String cmd = parts[0].toUpperCase();

        switch (cmd) {
            case "LIMIT" -> {
                Side side = Side.valueOf(parts[1].toUpperCase());
                long price = Long.parseLong(parts[2]);
                long qty = Long.parseLong(parts[3]);
                OrderCommand oc = OrderCommand.newLimit(side, price, qty);
                int tradesBefore = book.getTradeLog().size();
                book.processOrder(oc);
                printTrades(tradesBefore);
                System.out.printf("  #%d: LIMIT %s @ %d x %d%n",
                        oc.getOrder().getOrderId(), side, price, qty);
                printTop();
            }
            case "MARKET" -> {
                Side side = Side.valueOf(parts[1].toUpperCase());
                long qty = Long.parseLong(parts[2]);
                int tradesBefore = book.getTradeLog().size();
                book.processOrder(OrderCommand.newMarket(side, qty));
                printTrades(tradesBefore);
                System.out.printf("  #%d: MARKET %s x %d%n",
                        commandCount, side, qty);
                printTop();
            }
            case "CANCEL" -> {
                long orderId = Long.parseLong(parts[1]);
                book.processOrder(OrderCommand.newCancel(orderId));
                System.out.printf("  #%d: %s %d%n", commandCount, cmd, orderId);
                printTop();
            }
            case "MODIFY" -> {
                long orderId = Long.parseLong(parts[1]);
                long newPrice = Long.parseLong(parts[2]);
                long newQty = Long.parseLong(parts[3]);
                int tradesBefore = book.getTradeLog().size();
                book.processOrder(OrderCommand.newModify(orderId, newPrice, newQty));
                printTrades(tradesBefore);
                System.out.printf("  #%d: %s %d -> @ %d x %d%n",
                        commandCount, cmd, orderId, newPrice, newQty);
                printTop();
            }
            default -> System.out.printf("  Unknown command: %s%n", cmd);
        }
    }

    private void printTrades(int before) {
        List<Trade> trades = book.getTradeLog();
        for (int i = before; i < trades.size(); i++) {
            Trade t = trades.get(i);
            System.out.printf("  Trade: %d @ %d (maker=%d, taker=%d)%n",
                    t.getQuantity(), t.getPrice(),
                    t.getMakerOrderId(), t.getTakerOrderId());
        }
    }

    private void printTop() {
        TopOfBook top = book.getTopOfBook();
        String bid = top.hasBid() ? String.format("%d x %d", top.bestBid(), top.bidQuantity()) : "---";
        String ask = top.hasAsk() ? String.format("%d x %d", top.bestAsk(), top.askQuantity()) : "---";
        String spread = top.hasBoth() ? String.format("%d", top.spread()) : "N/A";
        System.out.printf("  Top: Bid=%s | Ask=%s | Spread=%s%n", bid, ask, spread);
    }

    private void printDepth() {
        TopOfBook top = book.getTopOfBook();
        System.out.println("--- Book Depth ---");
        System.out.printf("Best Bid: %s | Best Ask: %s | Spread: %s%n",
                top.hasBid() ? top.bestBid() : "---",
                top.hasAsk() ? top.bestAsk() : "---",
                top.hasBoth() ? top.spread() : "N/A");

        List<BookDepthEntry> bids = book.getBidDepth();
        List<BookDepthEntry> asks = book.getAskDepth();

        System.out.println("\nAsks:");
        if (asks.isEmpty()) {
            System.out.println("  (empty)");
        } else {
            for (int i = asks.size() - 1; i >= 0; i--) {
                BookDepthEntry e = asks.get(i);
                System.out.printf("  %d x %d (%d orders)%n", e.price(), e.quantity(), e.orderCount());
            }
        }

        System.out.println("---");
        System.out.println("Bids:");
        if (bids.isEmpty()) {
            System.out.println("  (empty)");
        } else {
            for (BookDepthEntry e : bids) {
                System.out.printf("  %d x %d (%d orders)%n", e.price(), e.quantity(), e.orderCount());
            }
        }
        System.out.println("---");
    }

    public static void main(String[] args) {
        InputStream in = args.length > 0
                ? CliRepl.class.getResourceAsStream(args[0])
                : System.in;
        if (in == null) {
            try {
                in = new FileInputStream(args[0]);
            } catch (FileNotFoundException e) {
                System.err.println("File not found: " + args[0]);
                System.exit(1);
            }
        }
        new CliRepl(in).run();
    }
}
