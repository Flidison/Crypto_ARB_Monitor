#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "crypto/CryptoArbitrageEngine.h"
#include "exceptions/Exceptions.h"

namespace {

std::string write_temp_csv(const std::string& name, const std::string& body) {
    const std::string path = "tmp_" + name + ".csv";
    std::ofstream out(path, std::ios::binary);
    out << body;
    out.close();
    return path;
}

std::string read_file(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

} // namespace

TEST(CryptoArbitrage, FindsNetPositiveOpportunityAfterFees) {
    const std::string qpath = write_temp_csv(
        "crypto_quotes_good",
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,BTCUSD,99.8,100.0,5\n"
        "KRAKEN,BTCUSD,101.2,101.3,5\n");

    am::CryptoArbitrageEngine engine;
    const auto quotes = engine.load_quotes_csv(qpath, {}, 10.0);
    std::remove(qpath.c_str());

    am::CryptoMonitorConfig cfg;
    cfg.symbol_filter = "BTCUSD";
    cfg.min_net_spread = 0.0;
    cfg.min_net_pct = 0.0;
    auto opps = engine.find_opportunities(quotes, cfg);

    EXPECT_EQ(opps.empty(), false);
    EXPECT_EQ(opps[0].net_spread > 0.0, true);
}

TEST(CryptoArbitrage, HighFeesRemoveFakeOpportunity) {
    const std::string qpath = write_temp_csv(
        "crypto_quotes_bad",
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,BTCUSD,99.95,100.0,30\n"
        "KRAKEN,BTCUSD,100.05,100.1,30\n");

    am::CryptoArbitrageEngine engine;
    const auto quotes = engine.load_quotes_csv(qpath, {}, 10.0);
    std::remove(qpath.c_str());

    am::CryptoMonitorConfig cfg;
    cfg.symbol_filter = "BTCUSD";
    auto opps = engine.find_opportunities(quotes, cfg);

    EXPECT_EQ(opps.size(), 0u);
}

TEST(CryptoArbitrage, DoesNotMixDifferentSymbols) {
    const std::string qpath = write_temp_csv(
        "crypto_quotes_multi_symbol",
        "exchange,symbol,bid,ask,fee_bps\n"
        "BINANCE,XTZUSD,10.9,11.0,5\n"
        "KRAKEN,APTUSD,12.0,12.1,5\n");

    am::CryptoArbitrageEngine engine;
    const auto quotes = engine.load_quotes_csv(qpath, {}, 10.0);
    std::remove(qpath.c_str());

    am::CryptoMonitorConfig cfg;
    cfg.symbol_filter = "";
    auto opps = engine.find_opportunities(quotes, cfg);

    EXPECT_EQ(opps.size(), 0u);
}

TEST(CryptoArbitrage, ProfitTrackingUsesBestNetPct) {
    const std::string ppath = write_temp_csv("profit_tracking", "");
    am::CryptoArbitrageEngine engine;
    engine.reset_profit_csv(ppath);

    std::vector<am::CryptoOpportunity> opps;
    opps.push_back(am::CryptoOpportunity{"XTZUSD", "BINANCE", "KRAKEN", 10.0, 10.3, 0.3, 0.2, 0.02});
    opps.push_back(am::CryptoOpportunity{"APTUSD", "KRAKEN", "BINANCE", 8.0, 8.1, 0.1, 0.05, 0.00625});

    const double after = engine.append_profit_csv(ppath, "2026-03-12T00:00:00Z", 1000.0, opps);
    EXPECT_NEAR(after, 1020.0, 1e-9);

    const std::string report = read_file(ppath);
    EXPECT_EQ(report.find("XTZUSD") != std::string::npos, true);
    std::remove(ppath.c_str());
}

TEST(CryptoArbitrage, ProfitTrackingKeepsCapitalWhenNoOpportunities) {
    const std::string ppath = write_temp_csv("profit_no_opps", "");
    am::CryptoArbitrageEngine engine;
    engine.reset_profit_csv(ppath);

    const std::vector<am::CryptoOpportunity> opps;
    const double after = engine.append_profit_csv(ppath, "2026-03-12T00:00:00Z", 1500.0, opps);
    EXPECT_NEAR(after, 1500.0, 1e-9);

    const std::string report = read_file(ppath);
    EXPECT_EQ(report.find(",0,") != std::string::npos, true);
    std::remove(ppath.c_str());
}

TEST(CryptoArbitrage, WriteOpportunitiesCsvRequiresValidObservedAt) {
    const std::string path = write_temp_csv("opps_with_timestamp", "");
    am::CryptoArbitrageEngine engine;
    std::vector<am::CryptoOpportunity> opps = {
        {"XTZUSD", "BINANCE", "KRAKEN", 10.0, 10.3, 0.3, 0.2, 0.02},
    };

    EXPECT_THROW(engine.write_opportunities_csv(path, "", opps), am::DataValidationError);
    EXPECT_THROW(engine.write_opportunities_csv(path, "not-a-time", opps), am::DataValidationError);

    engine.write_opportunities_csv(path, "2026-03-12T00:00:00Z", opps);
    const std::string report = read_file(path);
    EXPECT_EQ(report.find("2026-03-12T00:00:00Z") != std::string::npos, true);
    EXPECT_EQ(report.find(",XTZUSD,") != std::string::npos, true);
    std::remove(path.c_str());
}
