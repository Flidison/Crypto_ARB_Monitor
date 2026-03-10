#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "online/MarketDataConnectors.h"

TEST(TradingViewQuotes, PrefersRowsWithBidAskAndAppliesFeeMap) {
    // If duplicate rows exist, merge logic should prefer the row with full bid/ask.
    std::vector<am::TradingViewRowCandidate> rows = {
        {"BINANCE", "XTZUSD", std::nullopt, std::nullopt, 5.0},
        {"BINANCE", "XTZUSD", 4.90, 5.10, 5.0},
        {"KRAKEN", "XTZUSD", 5.00, 5.20, 5.1},
    };

    const std::unordered_map<std::string, double> fees = {
        {"BINANCE", 7.5},
    };

    const auto quotes = am::MarketDataConnectors::build_tradingview_quotes(
        rows, fees, 10.0, {"XTZUSD"}, {"BINANCE"});

    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].exchange, "BINANCE");
    EXPECT_EQ(quotes[0].symbol, "XTZUSD");
    EXPECT_NEAR(quotes[0].bid, 4.90, 1e-12);
    EXPECT_NEAR(quotes[0].ask, 5.10, 1e-12);
    EXPECT_NEAR(quotes[0].fee_bps, 7.5, 1e-12);
}

TEST(TradingViewQuotes, FallsBackToCloseWhenBidAskMissing) {
    // Close-only rows are accepted as synthetic bid/ask when scanner misses L1.
    std::vector<am::TradingViewRowCandidate> rows = {
        {"KRAKEN", "APTUSD", std::nullopt, std::nullopt, 12.34},
    };

    const auto quotes = am::MarketDataConnectors::build_tradingview_quotes(
        rows, {}, 10.0, {"APTUSD"}, {});

    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].exchange, "KRAKEN");
    EXPECT_EQ(quotes[0].symbol, "APTUSD");
    EXPECT_NEAR(quotes[0].bid, 12.34, 1e-12);
    EXPECT_NEAR(quotes[0].ask, 12.34, 1e-12);
    EXPECT_NEAR(quotes[0].fee_bps, 10.0, 1e-12);
}

TEST(TradingViewQuotes, FiltersOutRowsOutsideRequestedSymbols) {
    // Symbol filtering must be strict to avoid cross-symbol contamination.
    std::vector<am::TradingViewRowCandidate> rows = {
        {"BINANCE", "XTZUSD", 4.90, 5.10, 5.0},
        {"BINANCE", "ARBUSD", 1.00, 1.05, 1.02},
    };

    const auto quotes = am::MarketDataConnectors::build_tradingview_quotes(
        rows, {}, 10.0, {"ARBUSD"}, {});

    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].symbol, "ARBUSD");
}
