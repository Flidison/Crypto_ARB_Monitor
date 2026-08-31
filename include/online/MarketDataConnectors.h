#pragma once

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "crypto/CryptoArbitrageEngine.h"

namespace am {

struct TradingViewRowCandidate {
    std::string exchange;
    std::string symbol;
    std::optional<double> bid;
    std::optional<double> ask;
    std::optional<double> close;
};

class IMarketDataConnectors {
public:
    virtual ~IMarketDataConnectors() = default;

    virtual std::vector<CryptoQuote> fetch_btcusd_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const = 0;
    virtual std::vector<CryptoQuote> fetch_tradingview_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps,
        const std::vector<std::string>& symbols,
        const std::unordered_set<std::string>& allowed_exchanges) const = 0;
};

class MarketDataConnectors final : public IMarketDataConnectors {
public:
    using HttpGetFn = std::function<std::string(const std::string&)>;
    using HttpPostJsonFn = std::function<std::string(const std::string&, const std::string&)>;

    MarketDataConnectors();
    explicit MarketDataConnectors(HttpGetFn http_get_fn, HttpPostJsonFn http_post_json_fn);

    static std::optional<std::pair<double, double>> parse_binance_book_ticker(const std::string& json);
    static std::optional<std::pair<double, double>> parse_kraken_ticker(const std::string& json);
    static std::optional<std::pair<double, double>> parse_bitstamp_ticker(const std::string& json);

    static std::vector<CryptoQuote> build_tradingview_quotes(
        const std::vector<TradingViewRowCandidate>& rows,
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps,
        const std::vector<std::string>& symbols,
        const std::unordered_set<std::string>& allowed_exchanges);

    std::vector<CryptoQuote> fetch_btcusd_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const override;
    std::vector<CryptoQuote> fetch_tradingview_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps,
        const std::vector<std::string>& symbols,
        const std::unordered_set<std::string>& allowed_exchanges) const override;

private:
    static std::string shell_escape_single_quotes(const std::string& s);
    static std::optional<double> extract_json_number_field(const std::string& json, const std::string& field);
    static std::string curl_http_post_json(const std::string& url, const std::string& json_body);
    static std::string curl_http_get(const std::string& url);
    std::string http_post_json(const std::string& url, const std::string& json_body) const;
    std::string http_get(const std::string& url) const;

    HttpGetFn http_get_fn_;
    HttpPostJsonFn http_post_json_fn_;
};

} // namespace am
