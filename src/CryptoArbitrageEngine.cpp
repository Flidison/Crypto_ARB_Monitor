#include "crypto/CryptoArbitrageEngine.h"
#include "exceptions/Exceptions.h"
#include "common/StringUtils.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <vector>

namespace am {

namespace {

static char detect_delimiter(const std::string& header) {
    if (header.find('\t') != std::string::npos) return '\t';
    return ',';
}

static std::vector<std::string> split_csv_line(const std::string& line,
                                                char delim) {
    std::vector<std::string> out;
    std::stringstream ss(line);
    std::string tok;
    while (std::getline(ss, tok, delim))
        out.push_back(str::trim_copy(tok));
    return out;
}

static double parse_double(const std::string& raw, const char* field) {
    try { return std::stod(raw); }
    catch (...) {
        throw DataValidationError(
            std::string("Cannot parse '") + field + "': " + raw);
    }
}

} // anonymous namespace

std::vector<CryptoQuote> CryptoArbitrageEngine::load_quotes_csv(
    const std::string& path,
    const std::unordered_map<std::string, double>& fee_overrides,
    double default_fee_bps) const
{
    std::ifstream in(path);
    if (!in) throw CsvError("Cannot open quotes CSV: " + path);

    std::string hdr;
    if (!std::getline(in, hdr)) throw CsvError("Empty CSV: " + path);
    const char delim = detect_delimiter(hdr);
    const auto cols  = split_csv_line(hdr, delim);

    int c_ex = -1, c_sym = -1, c_bid = -1, c_ask = -1, c_fee = -1;
    for (int i = 0; i < (int)cols.size(); ++i) {
        auto h = str::upper_trim_copy(cols[i]);
        if      (h == "EXCHANGE") c_ex  = i;
        else if (h == "SYMBOL")   c_sym = i;
        else if (h == "BID")      c_bid = i;
        else if (h == "ASK")      c_ask = i;
        else if (h == "FEE_BPS")  c_fee = i;
    }
    if (c_ex < 0 || c_sym < 0 || c_bid < 0 || c_ask < 0)
        throw CsvError("Missing required columns in: " + path);

    std::vector<CryptoQuote> quotes;
    std::string line;
    while (std::getline(in, line)) {
        line = str::trim_copy(line);
        if (line.empty()) continue;
        const auto row = split_csv_line(line, delim);
        if ((int)row.size() <= std::max({c_ex, c_sym, c_bid, c_ask})) continue;

        CryptoQuote q;
        q.exchange = str::upper_trim_copy(row[c_ex]);
        q.symbol   = str::upper_trim_copy(row[c_sym]);
        q.bid      = parse_double(row[c_bid], "bid");
        q.ask      = parse_double(row[c_ask], "ask");
        if (q.bid <= 0.0 || q.ask <= 0.0 || q.ask < q.bid)
            throw DataValidationError("Bad bid/ask: " + q.exchange);

        if (c_fee >= 0 && c_fee < (int)row.size() && !row[c_fee].empty())
            q.fee_bps = parse_double(row[c_fee], "fee_bps");
        else {
            auto it = fee_overrides.find(q.exchange);
            q.fee_bps = (it != fee_overrides.end()) ? it->second
                                                     : default_fee_bps;
        }
        quotes.push_back(std::move(q));
    }
    return quotes;
}

std::vector<CryptoOpportunity> CryptoArbitrageEngine::find_opportunities(
    const std::vector<CryptoQuote>& quotes,
    const CryptoMonitorConfig& cfg) const
{
    std::vector<CryptoOpportunity> out;
    for (size_t i = 0; i < quotes.size(); ++i) {
        const auto& buy = quotes[i];
        if (!cfg.symbol_filter.empty() && buy.symbol != cfg.symbol_filter)
            continue;
        for (size_t j = 0; j < quotes.size(); ++j) {
            if (i == j) continue;
            const auto& sell = quotes[j];
            if (sell.symbol   != buy.symbol)  continue;
            if (sell.exchange == buy.exchange) continue;

            const double buy_cost  = buy.ask  * (1.0 + buy.fee_bps  / 10000.0);
            const double sell_gain = sell.bid  * (1.0 - sell.fee_bps / 10000.0);
            const double net       = sell_gain - buy_cost;
            if (net <= cfg.min_net_spread) continue;

            const double net_pct = net / buy_cost;
            if (net_pct <= cfg.min_net_pct) continue;

            CryptoOpportunity o;
            o.symbol        = buy.symbol;
            o.buy_exchange  = buy.exchange;
            o.sell_exchange = sell.exchange;
            o.buy_ask       = buy.ask;
            o.sell_bid      = sell.bid;
            o.gross_spread  = sell.bid - buy.ask;
            o.net_spread    = net;
            o.net_pct       = net_pct;
            out.push_back(std::move(o));
        }
    }
    std::sort(out.begin(), out.end(),
        [](const CryptoOpportunity& a, const CryptoOpportunity& b){
            return a.net_spread > b.net_spread;
        });
    return out;
}

