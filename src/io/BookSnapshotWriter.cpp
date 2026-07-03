#include "matchbox/io/BookSnapshotWriter.hpp"

namespace matchbox {

BookSnapshotWriter::BookSnapshotWriter(const std::filesystem::path& filePath) {
    bool exists = std::filesystem::exists(filePath);
    out_.open(filePath, std::ios::out | std::ios::app);
    if (!exists) {
        out_ << "side,price,quantity,orderCount\n";
        out_.flush();
    }
}

void BookSnapshotWriter::writeSnapshot(long timestamp, const TopOfBook& top,
                                       const std::vector<BookDepthEntry>& bids,
                                       const std::vector<BookDepthEntry>& asks) {
    out_ << "# snapshot_time=" << timestamp
         << ",bestBid=" << (top.hasBid() ? top.bestBid : 0)
         << ",bestAsk=" << (top.hasAsk() ? top.bestAsk : 0)
         << ",spread=" << (top.hasBoth() ? top.spread() : 0) << '\n';

    for (const BookDepthEntry& bid : bids) {
        out_ << "B," << bid.price << ',' << bid.quantity << ',' << bid.orderCount << '\n';
    }
    for (const BookDepthEntry& ask : asks) {
        out_ << "A," << ask.price << ',' << ask.quantity << ',' << ask.orderCount << '\n';
    }
    out_ << '\n';
    out_.flush();
}

void BookSnapshotWriter::close() {
    if (out_.is_open()) out_.close();
}

}
