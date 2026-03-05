#include <gtest/gtest.h>
#include "online/MarketDataConnectors.h"

TEST(TradingViewQuotes, PrefersRowsWithBidAskAndAppliesFeeMap) {
    std::vector<am::TradingViewRowCandidate> rows = {
        {"BINANCE", "BTCUSD", 65450.0, 65455.0, 65452.0},
        {"BINANCE", "BTCUSD", std::nullopt, std::nullopt, 65453.0},
    };
    std::unordered_map<std::string,double> fees = {{"BINANCE", 8.0}};
    auto out = am::MarketDataConnectors::build_tradingview_quotes(
        rows, fees, 10.0, {"BTCUSD"}, {"BINANCE"});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_DOUBLE_EQ(out[0].bid, 65450.0);
    EXPECT_DOUBLE_EQ(out[0].fee_bps, 8.0);
}

TEST(TradingViewQuotes, FallsBackToCloseWhenBidAskMissing) {
    std::vector<am::TradingViewRowCandidate> rows = {
        {"KRAKEN", "BTCUSD", std::nullopt, std::nullopt, 65460.0},
    };
    auto out = am::MarketDataConnectors::build_tradingview_quotes(
        rows, {}, 10.0, {}, {});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_DOUBLE_EQ(out[0].bid, 65460.0);
    EXPECT_DOUBLE_EQ(out[0].ask, 65460.0);
}

TEST(TradingViewQuotes, FiltersOutRowsOutsideRequestedSymbols) {
    std::vector<am::TradingViewRowCandidate> rows = {
        {"BINANCE", "ETHUSD", 3500.0, 3501.0, std::nullopt},
        {"BINANCE", "BTCUSD", 65450.0, 65455.0, std::nullopt},
    };
    auto out = am::MarketDataConnectors::build_tradingview_quotes(
        rows, {}, 10.0, {"BTCUSD"}, {});
    ASSERT_EQ(out.size(), 1u);
    EXPECT_EQ(out[0].symbol, "BTCUSD");
}
