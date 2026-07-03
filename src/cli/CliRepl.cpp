#include <cctype>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "matchbox/book/OrderBook.hpp"

using namespace matchbox;

// Interactive CLI for the order book.
//
// Usage:
//   matchbox_cli < orders.txt
//   matchbox_cli orders.txt
//   matchbox_cli                 (interactive stdin)
//
// Input format, one command per line:
//   LIMIT  <BUY|SELL> <price> <qty>
//   MARKET <BUY|SELL> <qty>
//   CANCEL <orderId>
//   MODIFY <orderId> <newPrice> <newQty>
//   # comment    (also //)
//   <blank line> prints the full depth snapshot
namespace {

std::string sideName(Side side) { return side == Side::BUY ? "BUY" : "SELL"; }

Side parseSide(const std::string& token) {
    std::string upper = token;
    for (char& c : upper) c = static_cast<char>(std::toupper(c));
    if (upper == "BUY") return Side::BUY;
    if (upper == "SELL") return Side::SELL;
    throw std::invalid_argument("bad side: " + token);
}

class CliRepl {
public:
    explicit CliRepl(std::istream& in) : in_(in) {}

    void run() {
        std::string line;
        while (std::getline(in_, line)) {
            std::string trimmed = trim(line);
            if (trimmed.empty()) {
                printDepth();
                continue;
            }
            if (trimmed.rfind('#', 0) == 0 || trimmed.rfind("//", 0) == 0) continue;

            ++commandCount_;
            try {
                processLine(trimmed);
            } catch (const std::exception& e) {
                std::cout << "  Error: " << e.what() << "\n";
            }
        }
    }

private:
    static std::string trim(const std::string& s) {
        size_t begin = s.find_first_not_of(" \t\r\n");
        if (begin == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(begin, end - begin + 1);
    }

    void processLine(const std::string& line) {
        std::istringstream iss(line);
        std::vector<std::string> parts;
        std::string tok;
        while (iss >> tok) parts.push_back(tok);

        std::string cmd = parts[0];
        for (char& c : cmd) c = static_cast<char>(std::toupper(c));

        if (cmd == "LIMIT") {
            Side side = parseSide(parts.at(1));
            long price = std::stol(parts.at(2));
            long qty = std::stol(parts.at(3));
            OrderCommand oc = OrderCommand::newLimit(side, price, qty);
            size_t before = book_.getTradeLog().size();
            book_.processOrder(oc);
            printTrades(before);
            std::cout << "  #" << oc.order()->orderId() << ": LIMIT " << sideName(side)
                      << " @ " << price << " x " << qty << "\n";
            printTop();
        } else if (cmd == "MARKET") {
            Side side = parseSide(parts.at(1));
            long qty = std::stol(parts.at(2));
            size_t before = book_.getTradeLog().size();
            book_.processOrder(OrderCommand::newMarket(side, qty));
            printTrades(before);
            std::cout << "  #" << commandCount_ << ": MARKET " << sideName(side)
                      << " x " << qty << "\n";
            printTop();
        } else if (cmd == "CANCEL") {
            long orderId = std::stol(parts.at(1));
            book_.processOrder(OrderCommand::newCancel(orderId));
            std::cout << "  #" << commandCount_ << ": CANCEL " << orderId << "\n";
            printTop();
        } else if (cmd == "MODIFY") {
            long orderId = std::stol(parts.at(1));
            long newPrice = std::stol(parts.at(2));
            long newQty = std::stol(parts.at(3));
            size_t before = book_.getTradeLog().size();
            book_.processOrder(OrderCommand::newModify(orderId, newPrice, newQty));
            printTrades(before);
            std::cout << "  #" << commandCount_ << ": MODIFY " << orderId << " -> @ "
                      << newPrice << " x " << newQty << "\n";
            printTop();
        } else {
            std::cout << "  Unknown command: " << cmd << "\n";
        }
    }

    void printTrades(size_t before) {
        const auto& trades = book_.getTradeLog();
        for (size_t i = before; i < trades.size(); ++i) {
            const Trade& t = trades[i];
            std::cout << "  Trade: " << t.quantity << " @ " << t.price
                      << " (maker=" << t.makerOrderId << ", taker=" << t.takerOrderId << ")\n";
        }
    }

    void printTop() {
        TopOfBook top = book_.getTopOfBook();
        std::cout << "  Top: Bid=";
        if (top.hasBid()) std::cout << top.bestBid << " x " << top.bidQuantity;
        else std::cout << "---";
        std::cout << " | Ask=";
        if (top.hasAsk()) std::cout << top.bestAsk << " x " << top.askQuantity;
        else std::cout << "---";
        std::cout << " | Spread=";
        if (top.hasBoth()) std::cout << top.spread();
        else std::cout << "N/A";
        std::cout << "\n";
    }

    void printDepth() {
        TopOfBook top = book_.getTopOfBook();
        std::cout << "--- Book Depth ---\n";
        std::cout << "Best Bid: " << (top.hasBid() ? std::to_string(top.bestBid) : "---")
                  << " | Best Ask: " << (top.hasAsk() ? std::to_string(top.bestAsk) : "---")
                  << " | Spread: " << (top.hasBoth() ? std::to_string(top.spread()) : "N/A")
                  << "\n";

        std::vector<BookDepthEntry> bids = book_.getBidDepth();
        std::vector<BookDepthEntry> asks = book_.getAskDepth();

        std::cout << "\nAsks:\n";
        if (asks.empty()) {
            std::cout << "  (empty)\n";
        } else {
            for (auto it = asks.rbegin(); it != asks.rend(); ++it) {
                std::cout << "  " << it->price << " x " << it->quantity
                          << " (" << it->orderCount << " orders)\n";
            }
        }

        std::cout << "---\nBids:\n";
        if (bids.empty()) {
            std::cout << "  (empty)\n";
        } else {
            for (const BookDepthEntry& e : bids) {
                std::cout << "  " << e.price << " x " << e.quantity
                          << " (" << e.orderCount << " orders)\n";
            }
        }
        std::cout << "---\n";
    }

    std::istream& in_;
    OrderBook book_;
    long commandCount_ = 0;
};

}  // namespace

int main(int argc, char** argv) {
    if (argc > 1) {
        std::ifstream file(argv[1]);
        if (!file) {
            std::cerr << "File not found: " << argv[1] << "\n";
            return 1;
        }
        CliRepl(file).run();
    } else {
        CliRepl(std::cin).run();
    }
    return 0;
}
