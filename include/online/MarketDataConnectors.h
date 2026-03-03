#pragma once
#include "crypto/CryptoArbitrageEngine.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

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
        const std::vector<std::string>& requested_symbols,
        const std::vector<std::string>& allowed_exchanges) const = 0;
};

class MarketDataConnectors final : public IMarketDataConnectors {
public:
    static std::optional<std::pair<double,double>>
        parse_binance_book_ticker(const std::string& json);
    static std::optional<std::pair<double,double>>
        parse_kraken_ticker(const std::string& json);
    static std::optional<std::pair<double,double>>
        parse_bitstamp_ticker(const std::string& json);

    static std::vector<TradingViewRowCandidate>
        parse_tradingview_scan_rows(const std::string& json);

    static std::vector<CryptoQuote> build_tradingview_quotes(
        const std::vector<TradingViewRowCandidate>& rows,
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps,
        const std::vector<std::string>& requested_symbols,
        const std::vector<std::string>& allowed_exchanges);

    std::vector<CryptoQuote> fetch_btcusd_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const override;

    std::vector<CryptoQuote> fetch_tradingview_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps,
        const std::vector<std::string>& requested_symbols,
        const std::vector<std::string>& allowed_exchanges) const override;

protected:
    virtual std::string http_get(const std::string& url) const;
    virtual std::string http_post(const std::string& url,
                                  const std::string& body) const;
private:
    static std::string curl_http_get(const std::string& url);
    static std::string curl_http_post(const std::string& url,
                                      const std::string& body);
};

} // namespace am
