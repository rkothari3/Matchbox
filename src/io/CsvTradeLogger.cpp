#include "matchbox/io/CsvTradeLogger.hpp"

namespace matchbox {

CsvTradeLogger::CsvTradeLogger(const std::filesystem::path& filePath) {
    bool exists = std::filesystem::exists(filePath);
    out_.open(filePath, std::ios::out | std::ios::app);
    if (!exists) {
        out_ << "timestamp,price,quantity,makerOrderId,takerOrderId\n";
        out_.flush();
    }
}

void CsvTradeLogger::appendTrade(const Trade& trade) {
    out_ << trade.timestamp << ',' << trade.price << ',' << trade.quantity << ','
         << trade.makerOrderId << ',' << trade.takerOrderId << '\n';
    out_.flush();
}

void CsvTradeLogger::close() {
    if (out_.is_open()) out_.close();
}

}
