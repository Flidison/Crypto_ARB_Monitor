#include "config/ConfigManager.h"
#include "crypto/CryptoArbitrageEngine.h"
#include "online/MarketDataConnectors.h"
#include "exceptions/Exceptions.h"

#include <chrono>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

namespace fs = std::filesystem;

static void print_usage() {
    std::cerr
        << "Usage: arbitrage_monitor <command> [options]\n"
        << "  run    [--config <path>]  Run once\n"
        << "  watch  [--config <path>]  Run in loop\n"
        << "  config [--config <path>]  Print config\n";
}

static std::string resolve_from_config(const std::string& cfg_path,
                                       const std::string& p) {
    if (fs::path(p).is_absolute()) return p;
    return (fs::path(cfg_path).parent_path() / p).string();
}

static std::string utc_now_iso8601() {
    auto now = std::chrono::system_clock::now();
    std::time_t t = std::chrono::system_clock::to_time_t(now);
    std::tm utc{}; gmtime_r(&t, &utc);
    std::ostringstream oss;
    oss << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
    return oss.str();
}

static std::vector<std::string> parse_csv_list(const std::string& csv) {
    std::vector<std::string> out;
    std::stringstream ss(csv);
    std::string tok;
    while (std::getline(ss, tok, ',')) {
        tok.erase(0, tok.find_first_not_of(" \t"));
        tok.erase(tok.find_last_not_of(" \t") + 1);
        if (!tok.empty()) out.push_back(tok);
    }
    return out;
}

struct EffectiveConfig {
    std::string crypto_quotes_csv;
    std::string crypto_output_csv;
    bool        online_crypto_enabled  = false;
    std::string crypto_online_source   = "DIRECT";
    std::string crypto_symbols_csv;
    std::string crypto_exchanges_csv;
    int         watch_interval_sec     = 5;
    double      crypto_default_fee_bps = 10.0;
};

static EffectiveConfig load_effective(const am::IConfigManager& cfg,
                                      const std::string& cfg_path) {
    EffectiveConfig ec;
    auto s = [&](const std::string& k, const std::string& d){
        auto v = cfg.get_string(k); return v ? *v : d;
    };
    ec.crypto_quotes_csv    = resolve_from_config(cfg_path,
        s("crypto_quotes_csv","fixture_crypto_quotes.csv"));
    ec.crypto_output_csv    = resolve_from_config(cfg_path,
        s("crypto_output_csv","crypto_opportunities.csv"));
    ec.crypto_online_source  = s("crypto_online_source","DIRECT");
    ec.crypto_symbols_csv    = s("crypto_symbols","");
    ec.crypto_exchanges_csv  = s("crypto_exchanges","");
    if (auto v = cfg.get_bool("online_crypto_enabled"))    ec.online_crypto_enabled  = *v;
    if (auto v = cfg.get_double("watch_interval_sec"))     ec.watch_interval_sec     = (int)*v;
    if (auto v = cfg.get_double("crypto_default_fee_bps")) ec.crypto_default_fee_bps = *v;
    return ec;
}

int main(int argc, char* argv[]) {
    const std::string cmd = (argc >= 2) ? argv[1] : "run";
    if (cmd != "run" && cmd != "watch" && cmd != "config") {
        print_usage(); return 1;
    }

    std::string config_path = "config/app.conf";
    for (int i = 2; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--config") config_path = argv[i + 1];

    try {
        am::ConfigManager cfg_mgr(config_path);
        cfg_mgr.load();
        const auto ec = load_effective(cfg_mgr, config_path);

        if (cmd == "config") {
            std::cout
                << "crypto_quotes_csv="    << ec.crypto_quotes_csv   << "\n"
                << "crypto_output_csv="    << ec.crypto_output_csv   << "\n"
                << "online_crypto_enabled="
                << (ec.online_crypto_enabled ? "true" : "false")      << "\n"
                << "crypto_online_source=" << ec.crypto_online_source << "\n"
                << "watch_interval_sec="   << ec.watch_interval_sec   << "\n";
            return 0;
        }

        am::CryptoArbitrageEngine engine;
        am::CryptoMonitorConfig ccfg;
        auto selected_symbols   = parse_csv_list(ec.crypto_symbols_csv);
        auto selected_exchanges = parse_csv_list(ec.crypto_exchanges_csv);
        std::vector<am::CryptoQuote> quotes;

        if (ec.online_crypto_enabled) {
            am::MarketDataConnectors conn;
            if (ec.crypto_online_source == "TRADINGVIEW")
                quotes = conn.fetch_tradingview_quotes(
                    {}, ec.crypto_default_fee_bps,
                    selected_symbols, selected_exchanges);
            else
                quotes = conn.fetch_btcusd_quotes({}, ec.crypto_default_fee_bps);
        } else {
            quotes = engine.load_quotes_csv(
                ec.crypto_quotes_csv, {}, ec.crypto_default_fee_bps);
        }

        if (!selected_symbols.empty()) {
            std::unordered_set<std::string> sym_set(
                selected_symbols.begin(), selected_symbols.end());
            quotes.erase(std::remove_if(quotes.begin(), quotes.end(),
                [&](const am::CryptoQuote& q){ return !sym_set.count(q.symbol); }),
                quotes.end());
        }

        const auto opps = engine.find_opportunities(quotes, ccfg);
        engine.write_opportunities_csv(
            ec.crypto_output_csv, utc_now_iso8601(), opps);
        std::cout << "Wrote " << opps.size()
                  << " opportunities -> " << ec.crypto_output_csv << "\n";

    } catch (const am::AmException& e) {
        std::cerr << "Error: " << e.what() << "\n"; return 1;
    }
    return 0;
}
