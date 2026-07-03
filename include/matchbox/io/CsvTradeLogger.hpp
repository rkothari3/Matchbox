#pragma once

#include <filesystem>
#include <fstream>

#include "matchbox/core/Trade.hpp"

namespace matchbox {

// Appends trades to a CSV file.
//
// Format: timestamp,price,quantity,makerOrderId,takerOrderId
// A header row is written when the file is first created.
//
// CSV is chosen over a binary format for this learning project:
//   - human-readable: trades open in any editor or spreadsheet;
//   - trivially parseable by standard tooling (awk, python, pandas);
//   - schema-loose: appending a field stays backward-compatible.
// Binary would be more compact but adds serialization complexity for
// marginal benefit at this stage.
class CsvTradeLogger {
public:
    explicit CsvTradeLogger(const std::filesystem::path& filePath);

    void appendTrade(const Trade& trade);
    void close();

    ~CsvTradeLogger() { close(); }

    CsvTradeLogger(const CsvTradeLogger&) = delete;
    CsvTradeLogger& operator=(const CsvTradeLogger&) = delete;

private:
    std::ofstream out_;
};

}
