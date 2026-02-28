#pragma once
#include <string>
#include <unordered_map>
#include <vector>

namespace am {

struct CryptoQuote {
    std::string exchange;
    std::string symbol;
    double bid     = 0.0;
    double ask     = 0.0;
    double fee_bps = 0.0;
};

struct CryptoOpportunity {
    std::string symbol;
    std::string buy_exchange;
    std::string sell_exchange;
    double buy_ask      = 0.0;
    double sell_bid     = 0.0;
    double gross_spread = 0.0;
    double net_spread   = 0.0;
    double net_pct      = 0.0;
};

struct CryptoMonitorConfig {
    std::string symbol_filter = "BTCUSD";
    double min_net_spread     = 0.0;
    double min_net_pct        = 0.0;
};

class IArbitrageEngine {
public:
    virtual ~IArbitrageEngine() = default;

    virtual std::vector<CryptoQuote> load_quotes_csv(
        const std::string& path,
        const std::unordered_map<std::string, double>& fee_overrides,
        double default_fee_bps) const = 0;

    virtual std::vector<CryptoOpportunity> find_opportunities(
        const std::vector<CryptoQuote>& quotes,
        const CryptoMonitorConfig& cfg) const = 0;

    virtual void write_opportunities_csv(
        const std::string& path,
        const std::string& observed_at,
        const std::vector<CryptoOpportunity>& opps) const = 0;
};

