package com.matchbox.io;

import com.matchbox.core.Trade;
import java.io.IOException;
import java.io.Writer;
import java.nio.file.Files;
import java.nio.file.Path;
import java.nio.file.StandardOpenOption;

/**
 * Appends trades to a CSV file.
 *
 * Format: timestamp,price,quantity,makerOrderId,takerOrderId
 * Includes a header row on creation.
 *
 * CSV chosen over binary for a learning project:
 * - Human-readable: trades can be inspected in any text editor or Excel
 * - Trivially parseable: standard tooling (awk, python, pandas) can consume it
 * - No schema coupling: adding fields is backward-compatible
 * - Binary would be more compact but adds serialization complexity with
 *   marginal benefit for a learning-stage persistence layer.
 */
public class CsvTradeLogger implements AutoCloseable {

    private final Writer writer;

    public CsvTradeLogger(Path filePath) throws IOException {
        boolean exists = Files.exists(filePath);
        this.writer = Files.newBufferedWriter(filePath,
                StandardOpenOption.CREATE, StandardOpenOption.APPEND);
        if (!exists) {
            writer.write("timestamp,price,quantity,makerOrderId,takerOrderId\n");
            writer.flush();
        }
    }

    public void appendTrade(Trade trade) throws IOException {
        writer.write(String.format("%d,%d,%d,%d,%d\n",
                trade.getTimestamp(),
                trade.getPrice(),
                trade.getQuantity(),
                trade.getMakerOrderId(),
                trade.getTakerOrderId()));
        writer.flush();
    }

    @Override
    public void close() throws IOException {
        writer.close();
    }
}
