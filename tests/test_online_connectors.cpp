#include <gtest/gtest.h>
#include "online/MarketDataConnectors.h"
#include "exceptions/Exceptions.h"

TEST(OnlineConnectors, ParsesBinanceBookTicker) {
    const std::string json =
        R"({"symbol":"BTCUSDT","bidPrice":"65450.10","askPrice":"65455.20"})";
    auto r = am::MarketDataConnectors::parse_binance_book_ticker(json);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->first,  65450.10);
    EXPECT_DOUBLE_EQ(r->second, 65455.20);
}

TEST(OnlineConnectors, ParsesBinanceReturnNullOnBadJson) {
    EXPECT_FALSE(
        am::MarketDataConnectors::parse_binance_book_ticker("{}").has_value());
}

TEST(OnlineConnectors, ParsesKrakenTicker) {
    const std::string json =
        R"({"result":{"XXBTZUSD":{"b":["65462.40",1],"a":["65468.10",1]}}})";
    auto r = am::MarketDataConnectors::parse_kraken_ticker(json);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->first,  65462.40);
    EXPECT_DOUBLE_EQ(r->second, 65468.10);
}

TEST(OnlineConnectors, ParsesBitstampTicker) {
    const std::string json =
        R"({"bid":"65458.00","ask":"65463.50","timestamp":"1700000000"})";
    auto r = am::MarketDataConnectors::parse_bitstamp_ticker(json);
    ASSERT_TRUE(r.has_value());
    EXPECT_DOUBLE_EQ(r->first,  65458.00);
    EXPECT_DOUBLE_EQ(r->second, 65463.50);
}

class MockConnectors : public am::MarketDataConnectors {
public:
    std::string injected_response;
protected:
    std::string http_get(const std::string&) const override {
        return injected_response;
    }
};

TEST(OnlineConnectors, FetchBtcusdQuotesFromInjectedHttp) {
    MockConnectors mc;
    mc.injected_response =
        R"({"symbol":"BTCUSDT","bidPrice":"65450.10","askPrice":"65455.20"})";
    auto quotes = mc.fetch_btcusd_quotes({}, 10.0);
    EXPECT_FALSE(quotes.empty());
    EXPECT_EQ(quotes[0].exchange, "BINANCE");
}
