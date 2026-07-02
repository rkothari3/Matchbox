package com.matchbox.io;

import com.matchbox.book.BookDepthEntry;
import com.matchbox.book.TopOfBook;
import java.io.IOException;
import java.io.Writer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;
import java.util.List;

/**
 * Appends full-depth book snapshots to a CSV file.
 *
 * Format:
 *   # snapshot_time=<epochNanos>
 *   B,price,quantity,orderCount
 *   A,price,quantity,orderCount
 *   (empty line)
 *
 * 'B' rows are bids, 'A' rows are asks. The '#' line marks the
 * start of each snapshot with its timestamp.
 *
 * CSV chosen for same rationale as CsvTradeLogger — a learning
 * project benefits from inspectability over raw I/O throughput.
 */
public class BookSnapshotWriter implements AutoCloseable {

    private final Writer writer;

    public BookSnapshotWriter(Path filePath) throws IOException {
        boolean exists = Files.exists(filePath);
        this.writer = Files.newBufferedWriter(filePath,
                StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        if (!exists) {
            writer.write("side,price,quantity,orderCount\n");
            writer.flush();
        }
    }

    public void writeSnapshot(long timestamp, TopOfBook top,
                              List<BookDepthEntry> bids,
                              List<BookDepthEntry> asks) throws IOException {
        writer.write(String.format("# snapshot_time=%d,bestBid=%d,bestAsk=%d,spread=%d\n",
                timestamp,
                top.hasBid() ? top.bestBid() : 0,
                top.hasAsk() ? top.bestAsk() : 0,
                top.hasBoth() ? top.spread() : 0));

        for (BookDepthEntry bid : bids) {
            writer.write(String.format("B,%d,%d,%d\n",
                    bid.price(), bid.quantity(), bid.orderCount()));
        }
        for (BookDepthEntry ask : asks) {
            writer.write(String.format("A,%d,%d,%d\n",
                    ask.price(), ask.quantity(), ask.orderCount()));
        }
        writer.write("\n");
        writer.flush();
    }

    @Override
    public void close() throws IOException {
        writer.close();
    }
}
