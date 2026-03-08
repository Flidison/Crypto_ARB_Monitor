#include "crypto/CryptoArbitrageEngine.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <string>
#include <unordered_map>
#include <vector>

#include "common/StringUtils.h"
#include "exceptions/Exceptions.h"

namespace am {

namespace {

constexpr double kBpsDenominator = 10000.0;

static bool is_utc_iso8601_timestamp(const std::string& ts) {
    // Expected format: YYYY-MM-DDTHH:MM:SSZ (20 chars).
    if (ts.size() != 20) return false;
    const auto is_digit_at = [&](size_t i) {
        return std::isdigit(static_cast<unsigned char>(ts[i])) != 0;
    };
    return is_digit_at(0) && is_digit_at(1) && is_digit_at(2) && is_digit_at(3) &&
           ts[4] == '-' &&
           is_digit_at(5) && is_digit_at(6) &&
           ts[7] == '-' &&
           is_digit_at(8) && is_digit_at(9) &&
           ts[10] == 'T' &&
           is_digit_at(11) && is_digit_at(12) &&
           ts[13] == ':' &&
           is_digit_at(14) && is_digit_at(15) &&
           ts[16] == ':' &&
           is_digit_at(17) && is_digit_at(18) &&
           ts[19] == 'Z';
}

static char detect_delimiter(const std::string& header) {
    const std::vector<char> candidates = {',', ';', '\t', '|'};
    size_t best = 0;
    char delimiter = ',';
    for (const char c : candidates) {
        const size_t cnt = static_cast<size_t>(std::count(header.begin(), header.end(), c));
        if (cnt > best) {
            best = cnt;
            delimiter = c;
        }
    }
    return delimiter;
}

static std::vector<std::string> split_csv_line(const std::string& line, char delimiter) {
    std::vector<std::string> out;
    std::string cell;
    bool in_quotes = false;
    for (size_t i = 0; i < line.size(); ++i) {
        const char ch = line[i];
        if (ch == '"') {
            if (in_quotes && i + 1 < line.size() && line[i + 1] == '"') {
                cell.push_back('"');
                ++i;
            } else {
                in_quotes = !in_quotes;
            }
            continue;
        }
        if (ch == delimiter && !in_quotes) {
            out.push_back(str::trim_copy(cell));
            cell.clear();
            continue;
        }
        cell.push_back(ch);
    }
    if (in_quotes) throw CsvError("Malformed CSV line in crypto quotes: unclosed quote");
    out.push_back(str::trim_copy(cell));
    return out;
}

static double parse_double(const std::string& raw, const char* field_name) {
    const std::string s = str::trim_copy(raw);
    if (s.empty()) throw DataValidationError(std::string("Empty numeric field: ") + field_name);
    char* end = nullptr;
    const double v = std::strtod(s.c_str(), &end);
    if (end == s.c_str() || *end != '\0' || !std::isfinite(v)) {
        throw DataValidationError(std::string("Bad numeric field: ") + field_name);
    }
    return v;
}

static std::unordered_map<std::string, size_t> header_index(const std::vector<std::string>& headers) {
    std::unordered_map<std::string, size_t> idx;
    for (size_t i = 0; i < headers.size(); ++i) idx[str::upper_copy(headers[i])] = i;
    return idx;
}

static std::optional<size_t> find_alias(const std::unordered_map<std::string, size_t>& idx,
                                        std::initializer_list<const char*> aliases) {
    for (const char* a : aliases) {
        const auto it = idx.find(str::upper_copy(a));
        if (it != idx.end()) return it->second;
    }
    return std::nullopt;
}

static size_t require_alias(const std::unordered_map<std::string, size_t>& idx,
                            const char* logical_name,
                            std::initializer_list<const char*> aliases) {
    const auto c = find_alias(idx, aliases);
    if (!c.has_value()) throw CsvError(std::string("Missing crypto column: ") + logical_name);
    return *c;
}

static std::string csv_escape(const std::string& in) {
    bool needs_quotes = false;
    std::string out;
    out.reserve(in.size() + 4);
    for (const char ch : in) {
        if (ch == '"' || ch == ',' || ch == '\n' || ch == '\r') needs_quotes = true;
        if (ch == '"') out.push_back('"');
        out.push_back(ch);
    }
    if (!needs_quotes) return out;
    return "\"" + out + "\"";
}

} // namespace

std::unordered_map<std::string, double> CryptoArbitrageEngine::load_fees_csv(const std::string& csv_path) const {
    if (csv_path.empty()) return {};
    std::ifstream in(csv_path);
    if (!in) throw CsvError("Cannot open crypto fees file: " + csv_path);

    std::string header;
    if (!std::getline(in, header)) throw CsvError("Empty crypto fees CSV: " + csv_path);
    const char delimiter = detect_delimiter(header);
    auto headers = split_csv_line(header, delimiter);
    const auto idx = header_index(headers);
    const size_t c_exchange = require_alias(idx, "exchange", {"exchange", "venue", "broker"});
    const size_t c_fee = require_alias(idx, "fee_bps", {"fee_bps", "commission_bps"});

    std::unordered_map<std::string, double> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (str::trim_copy(line).empty()) continue;
        auto cols = split_csv_line(line, delimiter);
        if (cols.size() != headers.size()) throw CsvError("Crypto fees row has wrong column count");
        const std::string ex = str::upper_trim_copy(cols[c_exchange]);
        const double fee = parse_double(cols[c_fee], "fee_bps");
        out[ex] = fee;
    }
    return out;
}

std::vector<CryptoQuote> CryptoArbitrageEngine::load_quotes_csv(
    const std::string& csv_path,
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    const double default_fee_bps) const {

    std::ifstream in(csv_path);
    if (!in) throw CsvError("Cannot open crypto quotes file: " + csv_path);

    std::string header;
    if (!std::getline(in, header)) throw CsvError("Empty crypto quotes CSV: " + csv_path);
    const char delimiter = detect_delimiter(header);
    auto headers = split_csv_line(header, delimiter);
    const auto idx = header_index(headers);

    const size_t c_exchange = require_alias(idx, "exchange", {"exchange", "venue", "broker"});
    const size_t c_symbol = require_alias(idx, "symbol", {"symbol", "ticker", "pair"});
    const size_t c_bid = require_alias(idx, "bid", {"bid", "bid_price"});
    const size_t c_ask = require_alias(idx, "ask", {"ask", "ask_price"});
    const auto c_fee = find_alias(idx, {"fee_bps", "commission_bps"});

    std::vector<CryptoQuote> out;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (str::trim_copy(line).empty()) continue;
        auto cols = split_csv_line(line, delimiter);
        if (cols.size() != headers.size()) throw CsvError("Crypto quote row has wrong column count");

        CryptoQuote q;
        q.exchange = str::upper_trim_copy(cols[c_exchange]);
        q.symbol = str::upper_trim_copy(cols[c_symbol]);
        q.bid = parse_double(cols[c_bid], "bid");
        q.ask = parse_double(cols[c_ask], "ask");
        if (q.bid <= 0.0 || q.ask <= 0.0 || q.ask < q.bid) {
            throw DataValidationError("Bad crypto bid/ask");
        }
        if (c_fee.has_value()) {
            q.fee_bps = parse_double(cols[*c_fee], "fee_bps");
        } else {
            const auto it = per_exchange_fee_bps.find(q.exchange);
            q.fee_bps = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
        }
        out.push_back(std::move(q));
    }
    return out;
}

std::vector<CryptoOpportunity> CryptoArbitrageEngine::find_opportunities(
    const std::vector<CryptoQuote>& quotes,
    const CryptoMonitorConfig& cfg) const {

    std::vector<CryptoQuote> filtered;
    filtered.reserve(quotes.size());
    const std::string symbol = str::upper_trim_copy(cfg.symbol_filter);
    for (const auto& q : quotes) {
        if (!symbol.empty() && str::upper_copy(q.symbol) != symbol) continue;
        if (!cfg.allowed_exchanges.empty() && cfg.allowed_exchanges.count(str::upper_copy(q.exchange)) == 0) continue;
        filtered.push_back(q);
    }

    std::vector<CryptoOpportunity> out;
    for (size_t i = 0; i < filtered.size(); ++i) {
        for (size_t j = 0; j < filtered.size(); ++j) {
            if (i == j) continue;
            const auto& buy = filtered[i];
            const auto& sell = filtered[j];
            if (str::upper_copy(buy.exchange) == str::upper_copy(sell.exchange)) continue;
            if (str::upper_copy(buy.symbol) != str::upper_copy(sell.symbol)) continue;

            const double buy_cost = buy.ask * (1.0 + buy.fee_bps / kBpsDenominator);
            const double sell_gain = sell.bid * (1.0 - sell.fee_bps / kBpsDenominator);
            const double gross = sell.bid - buy.ask;
            const double net = sell_gain - buy_cost;
            const double net_pct = (buy_cost > 0.0) ? (net / buy_cost) : 0.0;

            if (net <= 0.0) continue;
            if (net < cfg.min_net_spread) continue;
            if (net_pct < cfg.min_net_pct) continue;

            CryptoOpportunity o;
            o.symbol = buy.symbol;
            o.buy_exchange = buy.exchange;
            o.sell_exchange = sell.exchange;
            o.buy_ask = buy.ask;
            o.sell_bid = sell.bid;
            o.gross_spread = gross;
            o.net_spread = net;
            o.net_pct = net_pct;
            out.push_back(std::move(o));
        }
    }

    std::sort(out.begin(), out.end(), [](const CryptoOpportunity& a, const CryptoOpportunity& b) {
        return a.net_spread > b.net_spread;
    });
    return out;
}

void CryptoArbitrageEngine::reset_opportunities_csv(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw CsvError("Cannot write crypto opportunities report: " + path);
    out << "observed_at,symbol,buy_exchange,sell_exchange,buy_ask,sell_bid,gross_spread,net_spread,net_pct\n";
}

void CryptoArbitrageEngine::append_opportunities_csv(
    const std::string& path,
    const std::string& observed_at,
    const std::vector<CryptoOpportunity>& opps) const {

    std::ofstream out(path, std::ios::app);
    if (!out) throw CsvError("Cannot append crypto opportunities report: " + path);
    // Contract guard: each written row must have UTC timestamp in observed_at.
    if (!opps.empty() && !is_utc_iso8601_timestamp(observed_at)) {
        throw DataValidationError("observed_at must be UTC ISO 8601: YYYY-MM-DDTHH:MM:SSZ");
    }
    out << std::fixed << std::setprecision(8);
    for (const auto& o : opps) {
        out << csv_escape(observed_at) << ","
            << csv_escape(o.symbol) << ","
            << csv_escape(o.buy_exchange) << ","
            << csv_escape(o.sell_exchange) << ","
            << o.buy_ask << ","
            << o.sell_bid << ","
            << o.gross_spread << ","
            << o.net_spread << ","
            << o.net_pct << "\n";
    }
}

void CryptoArbitrageEngine::reset_profit_csv(const std::string& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out) throw CsvError("Cannot write crypto profit report: " + path);
    out << "observed_at,capital_before,opportunities_count,best_symbol,buy_exchange,sell_exchange,best_net_pct,estimated_profit,capital_after\n";
}

double CryptoArbitrageEngine::append_profit_csv(
    const std::string& path,
    const std::string& observed_at,
    const double capital_before,
    const std::vector<CryptoOpportunity>& opps) const {

    std::ofstream out(path, std::ios::app);
    if (!out) throw CsvError("Cannot append crypto profit report: " + path);
    out << std::fixed << std::setprecision(8);

    if (opps.empty()) {
        out << csv_escape(observed_at) << ","
            << capital_before << ","
            << 0 << ","
            << ",,,"
            << 0.0 << ","
            << 0.0 << ","
            << capital_before << "\n";
        return capital_before;
    }

    const auto best_it = std::max_element(opps.begin(), opps.end(), [](const CryptoOpportunity& a, const CryptoOpportunity& b) {
        return a.net_pct < b.net_pct;
    });
    const CryptoOpportunity& best = *best_it;
    const double estimated_profit = capital_before * best.net_pct;
    const double capital_after = capital_before + estimated_profit;

    out << csv_escape(observed_at) << ","
        << capital_before << ","
        << opps.size() << ","
        << csv_escape(best.symbol) << ","
        << csv_escape(best.buy_exchange) << ","
        << csv_escape(best.sell_exchange) << ","
        << best.net_pct << ","
        << estimated_profit << ","
        << capital_after << "\n";
    return capital_after;
}

void CryptoArbitrageEngine::write_opportunities_csv(
    const std::string& path,
    const std::string& observed_at,
    const std::vector<CryptoOpportunity>& opps) const {
    reset_opportunities_csv(path);
    append_opportunities_csv(path, observed_at, opps);
}

} // namespace am
