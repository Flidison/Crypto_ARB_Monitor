#pragma once
#include "crypto/CryptoArbitrageEngine.h"
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace am {

class IMarketDataConnectors {
public:
    virtual ~IMarketDataConnectors() = default;
    virtual std::vector<CryptoQuote> fetch_btcusd_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const = 0;
};

class MarketDataConnectors final : public IMarketDataConnectors {
public:
    static std::optional<std::pair<double,double>>
        parse_binance_book_ticker(const std::string& json);
    static std::optional<std::pair<double,double>>
        parse_kraken_ticker(const std::string& json);
    static std::optional<std::pair<double,double>>
        parse_bitstamp_ticker(const std::string& json);

    std::vector<CryptoQuote> fetch_btcusd_quotes(
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const override;

protected:
    virtual std::string http_get(const std::string& url) const;
private:
    static std::string curl_http_get(const std::string& url);
};

} // namespace am
