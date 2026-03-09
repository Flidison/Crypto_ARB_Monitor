#include <algorithm>
#include <chrono>
#include <cctype>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_set>
#include <variant>
#include <vector>

#include "common/StringUtils.h"
#include "config/ConfigManager.h"
#include "crypto/CryptoArbitrageEngine.h"
#include "exceptions/Exceptions.h"
#include "online/MarketDataConnectors.h"

namespace {

struct EffectiveConfig {
    std::string config_path = "config/app.conf";
    std::string crypto_quotes_csv;
    std::string crypto_fees_csv;
    std::string crypto_output_csv = "crypto_opportunities.csv";
    std::string crypto_symbol = "BTCUSD";
    std::string crypto_symbols_csv = "BTCUSD";
    std::string crypto_exchanges_csv = "BINANCE,KRAKEN,BITSTAMP,FXPRO,OANDA,PEPPERSTONE";
    std::string crypto_online_source = "TRADINGVIEW";
    std::string profit_output_csv = "crypto_profit.csv";
    double crypto_default_fee_bps = 10.0;
    double crypto_min_net_spread = 0.0;
    double crypto_min_net_pct = 0.0;
    double start_capital = 1000.0;
    int watch_interval_sec = 5;
    bool profit_calc_enabled = false;
    bool online_crypto_enabled = false;
    bool no_fixtures = false;
};

constexpr std::string_view kTradingViewSource = "TRADINGVIEW";

// Runtime source mode is explicit and type-safe via variant dispatch.
struct OfflineQuotesSource {};
struct TradingViewQuotesSource {};
struct DirectExchangeQuotesSource {};
using QuotesSource = std::variant<OfflineQuotesSource, TradingViewQuotesSource, DirectExchangeQuotesSource>;

QuotesSource detect_quotes_source(const EffectiveConfig& cfg) {
    if (!cfg.online_crypto_enabled) return OfflineQuotesSource{};
    if (am::str::upper_trim_copy(cfg.crypto_online_source) == kTradingViewSource) return TradingViewQuotesSource{};
    return DirectExchangeQuotesSource{};
}

bool parse_bool_cli(const std::string& s) {
    const std::string normalized = am::str::upper_trim_copy(s);
    if (normalized == "1" || normalized == "TRUE") return true;
    if (normalized == "0" || normalized == "FALSE") return false;
    throw am::ConfigError("Invalid bool override value: " + s);
}

std::string resolve_from_config(const std::string& config_path, const std::string& p) {
    if (p.empty()) return p;
    const std::filesystem::path pp(p);
    if (pp.is_absolute()) return pp.string();
    const std::filesystem::path cfg = std::filesystem::absolute(std::filesystem::path(config_path));
    const std::filesystem::path base = cfg.parent_path();
    return (base / pp).lexically_normal().string();
}

bool is_fixture_or_test_path(const std::string& p) {
    if (p.empty()) return false;
    const std::filesystem::path pp(p);
    std::string file = pp.filename().string();
    std::string full = pp.lexically_normal().generic_string();
    for (char& ch : file) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    for (char& ch : full) ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
    if (file.rfind("fixture_", 0) == 0) return true;
    if (file.rfind("test_", 0) == 0) return true;
    if (file.contains("_fixture")) return true;
    return full.contains("/tests/");
}

std::unordered_set<std::string> parse_csv_set(const std::string& csv) {
    std::unordered_set<std::string> out;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const std::string value = am::str::upper_trim_copy(token);
        if (!value.empty()) out.insert(value);
    }
    return out;
}

std::vector<std::string> parse_csv_list(const std::string& csv) {
    std::vector<std::string> out;
    std::unordered_set<std::string> seen;
    std::stringstream ss(csv);
    std::string token;
    while (std::getline(ss, token, ',')) {
        const std::string value = am::str::upper_trim_copy(token);
        if (value.empty()) continue;
        if (seen.insert(value).second) out.push_back(value);
    }
    return out;
}

std::string utc_now_iso8601() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    gmtime_s(&tm, &tt);
#else
    gmtime_r(&tt, &tm);
#endif
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%dT%H:%M:%SZ");
    return os.str();
}

void print_usage() {
    std::cout
        << "Usage:\n"
        << "  arbitrage_monitor run [--config config/app.conf] [--online-crypto true|false] [--no-fixtures true|false] [--start-capital 1000]\n"
        << "  arbitrage_monitor watch [--config config/app.conf] [--interval-sec 2] [--online-crypto true|false] [--no-fixtures true|false] [--start-capital 1000]\n"
        << "  arbitrage_monitor config [--config config/app.conf] [--online-crypto true|false] [--no-fixtures true|false] [--start-capital 1000]\n";
}

void print_effective_config(const EffectiveConfig& c) {
    std::cout << "command=config\n";
    std::cout << "config_path=" << std::filesystem::absolute(c.config_path).string() << "\n";
    std::cout << "crypto_quotes_csv=" << c.crypto_quotes_csv << "\n";
    std::cout << "crypto_fees_csv=" << c.crypto_fees_csv << "\n";
    std::cout << "crypto_output_csv=" << c.crypto_output_csv << "\n";
    std::cout << "profit_output_csv=" << c.profit_output_csv << "\n";
    std::cout << "crypto_symbol=" << c.crypto_symbol << "\n";
    std::cout << "crypto_symbols=" << c.crypto_symbols_csv << "\n";
    std::cout << "crypto_exchanges=" << c.crypto_exchanges_csv << "\n";
    std::cout << "crypto_default_fee_bps=" << c.crypto_default_fee_bps << "\n";
    std::cout << "crypto_min_net_spread=" << c.crypto_min_net_spread << "\n";
    std::cout << "crypto_min_net_pct=" << c.crypto_min_net_pct << "\n";
    std::cout << "start_capital=" << c.start_capital << "\n";
    std::cout << "crypto_online_source=" << c.crypto_online_source << "\n";
    std::cout << "watch_interval_sec=" << c.watch_interval_sec << "\n";
    std::cout << "profit_calc_enabled=" << (c.profit_calc_enabled ? "true" : "false") << "\n";
    std::cout << "online_crypto_enabled=" << (c.online_crypto_enabled ? "true" : "false") << "\n";
    std::cout << "no_fixtures=" << (c.no_fixtures ? "true" : "false") << "\n";
}

} // namespace

int main(int argc, char** argv) {
    try {
        const std::string cmd = (argc >= 2) ? argv[1] : "run";
        if (cmd != "run" && cmd != "watch" && cmd != "config") {
            print_usage();
            return 1;
        }

        std::string config_path = "config/app.conf";
        std::optional<int> interval_override;
        std::optional<bool> online_crypto_override;
        std::optional<bool> no_fixtures_override;
        std::optional<double> start_capital_override;

        // CLI parsing is strict to avoid silent typos in automation scripts.
        for (int i = 2; i < argc; ++i) {
            const std::string arg = argv[i];
            if (arg == "--config" && i + 1 < argc) {
                config_path = argv[++i];
            } else if (arg == "--interval-sec" && i + 1 < argc) {
                interval_override = std::stoi(argv[++i]);
            } else if (arg == "--online-crypto" && i + 1 < argc) {
                online_crypto_override = parse_bool_cli(argv[++i]);
            } else if (arg == "--no-fixtures" && i + 1 < argc) {
                no_fixtures_override = parse_bool_cli(argv[++i]);
            } else if (arg == "--start-capital" && i + 1 < argc) {
                start_capital_override = std::stod(argv[++i]);
            } else if (arg.rfind("--", 0) == 0) {
                throw am::ConfigError("Unknown CLI option: " + arg);
            } else {
                throw am::ConfigError("Unexpected positional argument: " + arg);
            }
        }

        std::unique_ptr<am::IConfigManager> config = std::make_unique<am::ConfigManager>(config_path);
        config->load();

        EffectiveConfig ec;
        ec.config_path = config_path;
        ec.crypto_quotes_csv = config->get_string("crypto_quotes_csv").value_or("");
        ec.crypto_fees_csv = config->get_string("crypto_fees_csv").value_or("");
        ec.crypto_output_csv = config->get_string("crypto_output_csv").value_or("crypto_opportunities.csv");
        ec.profit_output_csv = config->get_string("profit_output_csv").value_or("crypto_profit.csv");
        ec.crypto_symbol = am::str::upper_trim_copy(config->get_string("crypto_symbol").value_or("BTCUSD"));
        ec.crypto_symbols_csv = config->get_string("crypto_symbols").value_or(ec.crypto_symbol);
        ec.crypto_exchanges_csv = config->get_string("crypto_exchanges").value_or("BINANCE,KRAKEN,BITSTAMP,FXPRO,OANDA,PEPPERSTONE");
        ec.crypto_online_source = am::str::upper_trim_copy(config->get_string("crypto_online_source").value_or("TRADINGVIEW"));
        ec.crypto_default_fee_bps = config->get_double("crypto_default_fee_bps").value_or(10.0);
        ec.crypto_min_net_spread = config->get_double("crypto_min_net_spread").value_or(0.0);
        ec.crypto_min_net_pct = config->get_double("crypto_min_net_pct").value_or(0.0);
        ec.start_capital = config->get_double("start_capital").value_or(1000.0);
        ec.watch_interval_sec = static_cast<int>(config->get_double("watch_interval_sec").value_or(5.0));
        ec.profit_calc_enabled = config->get_bool("profit_calc_enabled").value_or(false);
        ec.online_crypto_enabled = config->get_bool("online_crypto_enabled").value_or(false);
        ec.no_fixtures = config->get_bool("no_fixtures").value_or(false);

        if (interval_override.has_value()) ec.watch_interval_sec = *interval_override;
        if (online_crypto_override.has_value()) ec.online_crypto_enabled = *online_crypto_override;
        if (no_fixtures_override.has_value()) ec.no_fixtures = *no_fixtures_override;
        if (start_capital_override.has_value()) ec.start_capital = *start_capital_override;
        if (ec.start_capital < 0.0) throw am::ConfigError("start_capital cannot be negative");

        ec.crypto_quotes_csv = resolve_from_config(ec.config_path, ec.crypto_quotes_csv);
        ec.crypto_fees_csv = resolve_from_config(ec.config_path, ec.crypto_fees_csv);
        ec.crypto_output_csv = resolve_from_config(ec.config_path, ec.crypto_output_csv);
        ec.profit_output_csv = resolve_from_config(ec.config_path, ec.profit_output_csv);

        if (ec.no_fixtures) {
            if (is_fixture_or_test_path(ec.crypto_quotes_csv)) ec.crypto_quotes_csv.clear();
            if (is_fixture_or_test_path(ec.crypto_fees_csv)) ec.crypto_fees_csv.clear();
        }

        if (cmd == "config") {
            print_effective_config(ec);
            return 0;
        }

        std::cout << "[CRYPTO_ONLY] ArbitrageMonitor is running in crypto-arbitrage mode only.\n";

        const auto out_path = std::filesystem::path(ec.crypto_output_csv);
        const auto out_dir = out_path.parent_path().empty() ? std::filesystem::path(".") : out_path.parent_path();
        std::filesystem::create_directories(out_dir);
        double rolling_capital = ec.start_capital;
        std::shared_ptr<am::IArbitrageEngine> engine = std::make_shared<am::CryptoArbitrageEngine>();
        {
            engine->reset_opportunities_csv(ec.crypto_output_csv);
            if (ec.profit_calc_enabled) {
                const auto profit_path = std::filesystem::path(ec.profit_output_csv);
                const auto profit_dir = profit_path.parent_path().empty() ? std::filesystem::path(".") : profit_path.parent_path();
                std::filesystem::create_directories(profit_dir);
                engine->reset_profit_csv(ec.profit_output_csv);
            }
        }

        // Single iteration unit used by both `run` and `watch`.
        auto execute_once = [&]() {
            const auto fees_map = engine->load_fees_csv(ec.crypto_fees_csv);
            auto selected_symbols = parse_csv_list(ec.crypto_symbols_csv);
            if (selected_symbols.empty()) selected_symbols = parse_csv_list(ec.crypto_symbol);
            if (selected_symbols.empty()) {
                throw am::ConfigError("At least one symbol must be provided via crypto_symbols or crypto_symbol");
            }
            std::unordered_set<std::string> selected_symbol_set(selected_symbols.begin(), selected_symbols.end());

            const QuotesSource source = detect_quotes_source(ec);
            auto quotes = std::visit([&](const auto& source_mode) -> std::vector<am::CryptoQuote> {
                using SourceMode = std::decay_t<decltype(source_mode)>;
                if constexpr (std::is_same_v<SourceMode, OfflineQuotesSource>) {
                    if (ec.crypto_quotes_csv.empty()) {
                        throw am::ConfigError("crypto_quotes_csv is required when online_crypto_enabled=false");
                    }
                    if (!std::filesystem::exists(ec.crypto_quotes_csv)) {
                        throw am::CsvError("Crypto quotes file not found: " + ec.crypto_quotes_csv);
                    }
                    return engine->load_quotes_csv(ec.crypto_quotes_csv, fees_map, ec.crypto_default_fee_bps);
                }
                std::unique_ptr<am::IMarketDataConnectors> online = std::make_unique<am::MarketDataConnectors>();
                if constexpr (std::is_same_v<SourceMode, TradingViewQuotesSource>) {
                    const auto allowed = parse_csv_set(ec.crypto_exchanges_csv);
                    return online->fetch_tradingview_quotes(fees_map, ec.crypto_default_fee_bps, selected_symbols, allowed);
                }
                return online->fetch_btcusd_quotes(fees_map, ec.crypto_default_fee_bps);
            }, source);
            quotes.erase(std::remove_if(quotes.begin(), quotes.end(), [&](const am::CryptoQuote& q) {
                return selected_symbol_set.count(am::str::upper_trim_copy(q.symbol)) == 0;
            }), quotes.end());

            am::CryptoMonitorConfig ccfg;
            ccfg.symbol_filter = (selected_symbols.size() == 1) ? selected_symbols.front() : "";
            ccfg.allowed_exchanges = parse_csv_set(ec.crypto_exchanges_csv);
            ccfg.default_fee_bps = ec.crypto_default_fee_bps;
            ccfg.min_net_spread = ec.crypto_min_net_spread;
            ccfg.min_net_pct = ec.crypto_min_net_pct;

            const auto opps = engine->find_opportunities(quotes, ccfg);
            const std::string observed_at = utc_now_iso8601();
            engine->append_opportunities_csv(ec.crypto_output_csv, observed_at, opps);
            if (ec.profit_calc_enabled) {
                const double before = rolling_capital;
                rolling_capital = engine->append_profit_csv(ec.profit_output_csv, observed_at, rolling_capital, opps);
                std::cout << "Profit calc enabled. Capital: " << before << " -> " << rolling_capital
                          << ". Appended profit report: " << ec.profit_output_csv << "\n";
            }

            if (quotes.size() < 2) {
                std::cerr << "Crypto monitor warning: less than two exchanges available in this iteration ("
                          << quotes.size() << ")\n";
            }
            for (const auto& o : opps) {
                std::cout << "[CRYPTO_ARB] " << o.symbol
                          << " buy@" << o.buy_exchange << " ask=" << o.buy_ask
                          << " sell@" << o.sell_exchange << " bid=" << o.sell_bid
                          << " net=" << o.net_spread
                          << " net_pct=" << o.net_pct << "\n";
            }
            std::cout << "Done. Quotes=" << quotes.size() << ", opportunities=" << opps.size()
                      << ". Appended report: " << ec.crypto_output_csv << "\n";
        };

        // Watch mode is intentionally resilient: one failed iteration does not terminate the loop.
        if (cmd == "watch") {
            while (true) {
                try {
                    execute_once();
                } catch (const std::exception& e) {
                    std::cerr << "Watch iteration failed: " << e.what() << "\n";
                }
                std::this_thread::sleep_for(std::chrono::seconds(std::max(1, ec.watch_interval_sec)));
            }
        }

        execute_once();
        return 0;
    } catch (const am::AmException& e) {
        std::cerr << "AM error: " << e.what() << "\n";
        return 2;
    } catch (const std::exception& e) {
        std::cerr << "Unexpected error: " << e.what() << "\n";
        return 3;
    }
}
