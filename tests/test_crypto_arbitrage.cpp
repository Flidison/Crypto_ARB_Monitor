#include <gtest/gtest.h>
#include "crypto/CryptoArbitrageEngine.h"
#include "exceptions/Exceptions.h"
#include <fstream>
#include <filesystem>

namespace fs = std::filesystem;

static std::string tmp_csv(const std::string& body) {
    auto p = fs::temp_directory_path() / "am_quotes_test.csv";
    std::ofstream f(p); f << body;
    return p.string();
}

// BINANCE ask=1000.00 fee=8bps  -> buy_cost=1000.80
// KRAKEN  bid=1010.00 fee=8bps  -> sell_gain=1009.19
// net = +8.39 > 0
TEST(CryptoArbitrage, FindsNetPositiveOpportunityAfterFees) {
    auto p = tmp_csv(
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,BTCUSD,999.00,1000.00,8.0\n"
        "KRAKEN,BTCUSD,1010.00,1011.00,8.0\n");
    am::CryptoArbitrageEngine eng;
    auto quotes = eng.load_quotes_csv(p, {}, 10.0);
    am::CryptoMonitorConfig cfg;
    auto opps = eng.find_opportunities(quotes, cfg);
    EXPECT_FALSE(opps.empty());
    EXPECT_GT(opps.front().net_spread, 0.0);
}

// fee=500bps (5%) сделает арбитраж убыточным
// BINANCE ask=1000.00 fee=500bps -> buy_cost=1050.00
// KRAKEN  bid=1010.00 fee=500bps -> sell_gain=959.50
// net = 959.50 - 1050.00 = -90.50 < 0
TEST(CryptoArbitrage, HighFeesRemoveFakeOpportunity) {
    auto p = tmp_csv(
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,BTCUSD,999.00,1000.00,500.0\n"
        "KRAKEN,BTCUSD,1010.00,1011.00,500.0\n");
    am::CryptoArbitrageEngine eng;
    auto quotes = eng.load_quotes_csv(p, {}, 10.0);
    am::CryptoMonitorConfig cfg;
    EXPECT_TRUE(eng.find_opportunities(quotes, cfg).empty());
}

TEST(CryptoArbitrage, DoesNotMixDifferentSymbols) {
    auto p = tmp_csv(
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,BTCUSD,999.00,1000.00,8.0\n"
        "KRAKEN,ETHUSD,1010.00,1011.00,8.0\n");
    am::CryptoArbitrageEngine eng;
    auto quotes = eng.load_quotes_csv(p, {}, 10.0);
    am::CryptoMonitorConfig cfg;
    cfg.symbol_filter = "BTCUSD";
    EXPECT_TRUE(eng.find_opportunities(quotes, cfg).empty());
}

TEST(CryptoArbitrage, ThrowsOnBadTimestamp) {
    am::CryptoArbitrageEngine engine;
    std::vector<am::CryptoOpportunity> opps(1);
    auto path = (fs::temp_directory_path() / "out.csv").string();
    EXPECT_THROW(
        engine.write_opportunities_csv(path, "bad-timestamp", opps),
        am::DataValidationError);
}
