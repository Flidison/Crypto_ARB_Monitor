#include <gtest/gtest.h>

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "exceptions/Exceptions.h"
#include "online/MarketDataConnectors.h"

TEST(OnlineConnectors, ParsesBinanceBookTicker) {
    const std::string json =
        R"({"symbol":"BTCUSDT","bidPrice":"65450.10","bidQty":"1.0","askPrice":"65455.20","askQty":"1.1"})";
    const auto ba = am::MarketDataConnectors::parse_binance_book_ticker(json);
    ASSERT_EQ(ba.has_value(), true);
    EXPECT_NEAR(ba->first, 65450.10, 1e-9);
    EXPECT_NEAR(ba->second, 65455.20, 1e-9);
}

TEST(OnlineConnectors, ParsesKrakenTicker) {
    const std::string json =
        R"({"error":[],"result":{"XXBTZUSD":{"a":["65468.10000","1","1.000"],"b":["65462.40000","1","1.000"]}}})";
    const auto ba = am::MarketDataConnectors::parse_kraken_ticker(json);
    ASSERT_EQ(ba.has_value(), true);
    EXPECT_NEAR(ba->first, 65462.4, 1e-9);
    EXPECT_NEAR(ba->second, 65468.1, 1e-9);
}

TEST(OnlineConnectors, ParsesBitstampTicker) {
    const std::string json = R"({"bid":"65458.30","ask":"65464.90"})";
    const auto ba = am::MarketDataConnectors::parse_bitstamp_ticker(json);
    ASSERT_EQ(ba.has_value(), true);
    EXPECT_NEAR(ba->first, 65458.3, 1e-9);
    EXPECT_NEAR(ba->second, 65464.9, 1e-9);
}

TEST(OnlineConnectors, FetchBtcusdQuotesFromInjectedHttp) {
    // Integration-level fetch test without real network: URLs are served by lambdas.
    am::MarketDataConnectors c(
        [](const std::string& url) -> std::string {
            if (url.find("binance") != std::string::npos) {
                return R"({"symbol":"BTCUSDT","bidPrice":"65450.10","askPrice":"65455.20"})";
            }
            if (url.find("kraken") != std::string::npos) {
                return R"({"error":[],"result":{"XXBTZUSD":{"a":["65468.10000","1","1.000"],"b":["65462.40000","1","1.000"]}}})";
            }
            if (url.find("bitstamp") != std::string::npos) {
                return R"({"bid":"65458.30","ask":"65464.90"})";
            }
            throw am::CsvError("unexpected url");
        },
        [](const std::string&, const std::string&) -> std::string {
            throw am::CsvError("POST should not be called in this test");
        });

    const std::unordered_map<std::string, double> fees = {
        {"BINANCE", 8.0},
        {"KRAKEN", 9.0},
        {"BITSTAMP", 10.0},
    };
    const auto quotes = c.fetch_btcusd_quotes(fees, 12.0);

    ASSERT_EQ(quotes.size(), 3u);
    EXPECT_EQ(quotes[0].symbol, "BTCUSD");
    EXPECT_EQ(quotes[1].symbol, "BTCUSD");
    EXPECT_EQ(quotes[2].symbol, "BTCUSD");
}

TEST(OnlineConnectors, FetchTradingViewQuotesFromScannerRows) {
    // Scanner path should build normalized quotes and apply fee fallback.
    am::MarketDataConnectors c(
        [](const std::string&) -> std::string {
            throw am::CsvError("GET fallback should not be called in this test");
        },
        [](const std::string& url, const std::string&) -> std::string {
            if (url.find("/crypto/scan") != std::string::npos) {
                return R"({"data":[{"s":"BINANCE:XTZUSD","d":["XTZUSD","BINANCE",4.90,5.10,5.00,"x"]},{"s":"KRAKEN:XTZUSD","d":["XTZUSD","KRAKEN",5.00,5.20,5.10,"x"]}]})";
            }
            return R"({"data":[]})";
        });

    const auto quotes = c.fetch_tradingview_quotes(
        {{"BINANCE", 7.5}}, 10.0, {"XTZUSD"}, {"BINANCE", "KRAKEN"});
    ASSERT_EQ(quotes.size(), 2u);

    EXPECT_EQ(quotes[0].exchange, "BINANCE");
    EXPECT_EQ(quotes[0].symbol, "XTZUSD");
    EXPECT_NEAR(quotes[0].fee_bps, 7.5, 1e-12);

    EXPECT_EQ(quotes[1].exchange, "KRAKEN");
    EXPECT_EQ(quotes[1].symbol, "XTZUSD");
    EXPECT_NEAR(quotes[1].fee_bps, 10.0, 1e-12);
}

TEST(OnlineConnectors, FetchTradingViewQuotesFallsBackToSymbolPage) {
    // If scanner returns no rows, symbol page fallback should still produce quotes.
    am::MarketDataConnectors c(
        [](const std::string& url) -> std::string {
            if (url.find("symbols/XTZUSD") != std::string::npos) {
                return R"(meta "proName":"BINANCE:XTZUSD" the current price of XTZUSD is 12.34USD right now)";
            }
            throw am::CsvError("unexpected GET url");
        },
        [](const std::string&, const std::string&) -> std::string {
            return R"({"data":[]})";
        });

    const auto quotes = c.fetch_tradingview_quotes({}, 10.0, {"XTZUSD"}, {"BINANCE"});
    ASSERT_EQ(quotes.size(), 1u);
    EXPECT_EQ(quotes[0].exchange, "BINANCE");
    EXPECT_EQ(quotes[0].symbol, "XTZUSD");
    EXPECT_NEAR(quotes[0].bid, 12.34, 1e-12);
    EXPECT_NEAR(quotes[0].ask, 12.34, 1e-12);
}

TEST(OnlineConnectors, FetchTradingViewFallbackRejectsMismatchedTicker) {
    // A fallback page can contain price text but refer to a different venue symbol.
    am::MarketDataConnectors c(
        [](const std::string& url) -> std::string {
            if (url.find("symbols/ARBUSD") != std::string::npos) {
                return R"(meta "proName":"COINBASE:ARBUSD" the current price of ARBUSD is 0.0993USD right now)";
            }
            throw am::CsvError("unexpected GET url");
        },
        [](const std::string&, const std::string&) -> std::string {
            return R"({"data":[]})";
        });

    EXPECT_THROW(
        (void)c.fetch_tradingview_quotes({}, 10.0, {"ARBUSD"}, {"OANDA"}),
        am::CsvError);
}

TEST(OnlineConnectors, FetchTradingViewQuotesThrowsWhenAllSourcesFail) {
    // Error contract: if both scanner and page fallback fail, method throws CsvError.
    am::MarketDataConnectors c(
        [](const std::string&) -> std::string { throw am::CsvError("get failed"); },
        [](const std::string&, const std::string&) -> std::string { throw am::CsvError("post failed"); });

    EXPECT_THROW(
        (void)c.fetch_tradingview_quotes({}, 10.0, {"XTZUSD"}, {"BINANCE"}),
        am::CsvError);
}
