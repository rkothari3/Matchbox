#pragma once

#include <filesystem>
#include <fstream>
#include <vector>

#include "matchbox/book/BookDepthEntry.hpp"
#include "matchbox/book/TopOfBook.hpp"

namespace matchbox {

// Appends full-depth book snapshots to a CSV file.
//
// Format:
//   # snapshot_time=<nanos>,bestBid=<p>,bestAsk=<p>,spread=<s>
//   B,price,quantity,orderCount        (one per bid level)
//   A,price,quantity,orderCount        (one per ask level)
//   <blank line>
//
// 'B'/'A' prefix the side; the '#' line delimits each snapshot with its
// timestamp and top-of-book. CSV for the same reasons as CsvTradeLogger:
// inspectability beats raw I/O throughput for a learning-stage layer.
class BookSnapshotWriter {
public:
    explicit BookSnapshotWriter(const std::filesystem::path& filePath);

    void writeSnapshot(long timestamp, const TopOfBook& top,
                       const std::vector<BookDepthEntry>& bids,
                       const std::vector<BookDepthEntry>& asks);
    void close();

    ~BookSnapshotWriter() { close(); }

    BookSnapshotWriter(const BookSnapshotWriter&) = delete;
    BookSnapshotWriter& operator=(const BookSnapshotWriter&) = delete;

private:
    std::ofstream out_;
};

}
