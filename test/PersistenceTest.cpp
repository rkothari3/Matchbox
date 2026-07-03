#include "matchbox/io/BookSnapshotWriter.hpp"
#include "matchbox/io/CsvTradeLogger.hpp"

#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "matchbox/book/OrderBook.hpp"
#include "matchbox/core/Clock.hpp"

using namespace matchbox;

namespace {

// Read a file into lines split on '\n' (a trailing "\n\n" yields a final empty
// line; a single trailing "\n" does not).
std::vector<std::string> readLines(const std::filesystem::path& path) {
    std::ifstream in(path);
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(in, line)) lines.push_back(line);
    return lines;
}

bool contains(const std::string& haystack, const std::string& needle) {
    return haystack.find(needle) != std::string::npos;
}

bool startsWith(const std::string& s, const std::string& prefix) {
    return s.rfind(prefix, 0) == 0;
}

class PersistenceTest : public ::testing::Test {
protected:
    std::filesystem::path tempDir;

    void SetUp() override {
        const auto* info = ::testing::UnitTest::GetInstance()->current_test_info();
        tempDir = std::filesystem::temp_directory_path() /
                  (std::string("matchbox_") + info->name());
        std::filesystem::remove_all(tempDir);
        std::filesystem::create_directories(tempDir);
    }
    void TearDown() override { std::filesystem::remove_all(tempDir); }
};

TEST_F(PersistenceTest, TradeLoggerCreatesHeaderOnNewFile) {
    auto file = tempDir / "trades.csv";
    {
        CsvTradeLogger logger(file);
        logger.appendTrade(Trade{100, 10, 1000L, 1L, 2L});
    }
    auto lines = readLines(file);
    ASSERT_EQ(2u, lines.size());
    EXPECT_EQ("timestamp,price,quantity,makerOrderId,takerOrderId", lines[0]);
    EXPECT_TRUE(startsWith(lines[1], "1000,100,10,1,2"));
}

TEST_F(PersistenceTest, TradeLoggerAppendsMultipleTrades) {
    auto file = tempDir / "trades.csv";
    {
        CsvTradeLogger logger(file);
        logger.appendTrade(Trade{100, 5, 1000L, 1L, 2L});
        logger.appendTrade(Trade{101, 3, 2000L, 3L, 4L});
    }
    auto lines = readLines(file);
    ASSERT_EQ(3u, lines.size());
    EXPECT_TRUE(contains(lines[1], ",100,5,1,2"));
    EXPECT_TRUE(contains(lines[2], ",101,3,3,4"));
}

TEST_F(PersistenceTest, TradeLoggerReusesExistingFileWithoutDuplicateHeader) {
    auto file = tempDir / "trades.csv";
    {
        CsvTradeLogger logger(file);
        logger.appendTrade(Trade{100, 5, 1000L, 1L, 2L});
    }
    {
        CsvTradeLogger logger2(file);
        logger2.appendTrade(Trade{101, 3, 2000L, 3L, 4L});
    }
    auto lines = readLines(file);
    ASSERT_EQ(3u, lines.size());
    EXPECT_EQ("timestamp,price,quantity,makerOrderId,takerOrderId", lines[0]);
}

TEST_F(PersistenceTest, SnapshotWriterCreatesHeaderOnNewFile) {
    auto file = tempDir / "snapshots.csv";
    {
        BookSnapshotWriter writer(file);
        writer.writeSnapshot(1000L, TopOfBook::empty(), {}, {});
    }
    auto lines = readLines(file);
    ASSERT_EQ(3u, lines.size());
    EXPECT_EQ("side,price,quantity,orderCount", lines[0]);
    EXPECT_TRUE(startsWith(lines[1], "# snapshot_time=1000"));
}

TEST_F(PersistenceTest, SnapshotWriterRecordsBidsAndAsks) {
    auto file = tempDir / "snapshots.csv";
    {
        BookSnapshotWriter writer(file);
        writer.writeSnapshot(1000L, TopOfBook{100, 101, 20, 15},
                             {BookDepthEntry{100, 20, 2}},
                             {BookDepthEntry{101, 15, 1}});
    }
    auto lines = readLines(file);
    EXPECT_EQ("side,price,quantity,orderCount", lines[0]);
    EXPECT_TRUE(contains(lines[1], "bestBid=100,bestAsk=101"));
    EXPECT_TRUE(contains(lines[2], "B,100,20,2"));
    EXPECT_TRUE(contains(lines[3], "A,101,15,1"));
}

TEST_F(PersistenceTest, EndToEndPersistenceWorkflow) {
    auto tradeFile = tempDir / "trades.csv";
    auto snapFile = tempDir / "snapshots.csv";
    OrderBook book;

    book.processOrder(OrderCommand::newLimit(Side::SELL, 100, 10));
    book.processOrder(OrderCommand::newLimit(Side::SELL, 101, 5));
    book.processOrder(OrderCommand::newLimit(Side::BUY, 100, 8));

    {
        CsvTradeLogger tradeLog(tradeFile);
        BookSnapshotWriter snapWriter(snapFile);
        for (const Trade& trade : book.getTradeLog()) tradeLog.appendTrade(trade);
        snapWriter.writeSnapshot(nowNanos(), book.getTopOfBook(),
                                 book.getBidDepth(), book.getAskDepth());
    }

    auto tradeLines = readLines(tradeFile);
    ASSERT_EQ(2u, tradeLines.size());
    EXPECT_EQ("timestamp,price,quantity,makerOrderId,takerOrderId", tradeLines[0]);
    EXPECT_TRUE(contains(tradeLines[1], ",100,8,"));

    auto snapLines = readLines(snapFile);
    EXPECT_TRUE(contains(snapLines[0], "side"));
    EXPECT_TRUE(contains(snapLines[2], "A,100,2"));
    EXPECT_TRUE(contains(snapLines[3], "A,101,5"));
}

TEST_F(PersistenceTest, TopOfBookEmptyEncodedCorrectly) {
    auto file = tempDir / "snapshots.csv";
    {
        BookSnapshotWriter writer(file);
        writer.writeSnapshot(5000L, TopOfBook::empty(), {}, {});
    }
    auto lines = readLines(file);
    const std::string& header = lines[1];
    EXPECT_TRUE(contains(header, "bestBid=0"));
    EXPECT_TRUE(contains(header, "bestAsk=0"));
    EXPECT_TRUE(contains(header, "spread=0"));
}

}  // namespace
