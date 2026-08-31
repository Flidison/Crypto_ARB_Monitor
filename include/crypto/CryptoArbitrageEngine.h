#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace am {

struct CryptoQuote {
    std::string exchange;
    std::string symbol;
    double bid = 0.0;
    double ask = 0.0;
    double fee_bps = 0.0;
};

struct CryptoOpportunity {
    std::string symbol;
    std::string buy_exchange;
    std::string sell_exchange;
    double buy_ask = 0.0;
    double sell_bid = 0.0;
    double gross_spread = 0.0;
    double net_spread = 0.0;
    double net_pct = 0.0;
};

struct CryptoMonitorConfig {
    std::string symbol_filter = "BTCUSD";
    std::unordered_set<std::string> allowed_exchanges;
    double default_fee_bps = 10.0;
    double min_net_spread = 0.0;
    double min_net_pct = 0.0;
};

class IArbitrageEngine {
public:
    virtual ~IArbitrageEngine() = default;

    virtual std::vector<CryptoQuote> load_quotes_csv(
        const std::string& csv_path,
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const = 0;

    virtual std::unordered_map<std::string, double> load_fees_csv(const std::string& csv_path) const = 0;

    virtual std::vector<CryptoOpportunity> find_opportunities(
        const std::vector<CryptoQuote>& quotes,
        const CryptoMonitorConfig& cfg) const = 0;

    virtual void reset_opportunities_csv(const std::string& path) const = 0;
    virtual void append_opportunities_csv(
        const std::string& path,
        const std::string& observed_at,
        const std::vector<CryptoOpportunity>& opps) const = 0;
    virtual void reset_profit_csv(const std::string& path) const = 0;
    virtual double append_profit_csv(
        const std::string& path,
        const std::string& observed_at,
        double capital_before,
        const std::vector<CryptoOpportunity>& opps) const = 0;

    virtual void write_opportunities_csv(
        const std::string& path,
        const std::string& observed_at,
        const std::vector<CryptoOpportunity>& opps) const = 0;
};

class CryptoArbitrageEngine final : public IArbitrageEngine {
public:
    std::vector<CryptoQuote> load_quotes_csv(
        const std::string& csv_path,
        const std::unordered_map<std::string, double>& per_exchange_fee_bps,
        double default_fee_bps) const override;

    std::unordered_map<std::string, double> load_fees_csv(const std::string& csv_path) const override;

    std::vector<CryptoOpportunity> find_opportunities(
        const std::vector<CryptoQuote>& quotes,
        const CryptoMonitorConfig& cfg) const override;

    void reset_opportunities_csv(const std::string& path) const override;
    void append_opportunities_csv(
        const std::string& path,
        const std::string& observed_at,
        const std::vector<CryptoOpportunity>& opps) const override;
    void reset_profit_csv(const std::string& path) const override;
    double append_profit_csv(
        const std::string& path,
        const std::string& observed_at,
        double capital_before,
        const std::vector<CryptoOpportunity>& opps) const override;

    void write_opportunities_csv(
        const std::string& path,
        const std::string& observed_at,
        const std::vector<CryptoOpportunity>& opps) const override;
};

} // namespace am
