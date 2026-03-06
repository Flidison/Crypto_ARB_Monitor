#include "online/MarketDataConnectors.h"
#include "exceptions/Exceptions.h"
#include "common/StringUtils.h"
#include <algorithm>
#include <sstream>
#include <unordered_set>

namespace am {

namespace {

static std::optional<double> parse_number_token(const std::string& t) {
    try {
        size_t pos = 0;
        double v = std::stod(t, &pos);
        return pos > 0 ? std::optional<double>(v) : std::nullopt;
    } catch (...) { return std::nullopt; }
}

static std::optional<std::string> extract_json_str(
    const std::string& json, const std::string& key)
{
    const std::string needle = "\"" + key + "\":\"";
    auto p = json.find(needle);
    if (p == std::string::npos) return std::nullopt;
    p += needle.size();
    auto e = json.find('"', p);
    if (e == std::string::npos) return std::nullopt;
    return json.substr(p, e - p);
}

} // anonymous namespace

std::optional<std::pair<double,double>>
MarketDataConnectors::parse_binance_book_ticker(const std::string& json) {
    auto b = extract_json_str(json, "bidPrice");
    auto a = extract_json_str(json, "askPrice");
    if (!b || !a) return std::nullopt;
    auto bid = parse_number_token(*b);
    auto ask = parse_number_token(*a);
    if (!bid || !ask || *bid <= 0 || *ask <= 0) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::optional<std::pair<double,double>>
MarketDataConnectors::parse_kraken_ticker(const std::string& json) {
    auto bp = json.find("\"b\":[\""); auto ap = json.find("\"a\":[\"");
    if (bp == std::string::npos || ap == std::string::npos) return std::nullopt;
    bp += 6; auto be = json.find('"', bp);
    ap += 6; auto ae = json.find('"', ap);
    if (be == std::string::npos || ae == std::string::npos) return std::nullopt;
    auto bid = parse_number_token(json.substr(bp, be - bp));
    auto ask = parse_number_token(json.substr(ap, ae - ap));
    if (!bid || !ask || *bid <= 0 || *ask <= 0) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::optional<std::pair<double,double>>
MarketDataConnectors::parse_bitstamp_ticker(const std::string& json) {
    auto b = extract_json_str(json, "bid");
    auto a = extract_json_str(json, "ask");
    if (!b || !a) return std::nullopt;
    auto bid = parse_number_token(*b);
    auto ask = parse_number_token(*a);
    if (!bid || !ask || *bid <= 0 || *ask <= 0) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::vector<TradingViewRowCandidate>
MarketDataConnectors::parse_tradingview_scan_rows(const std::string& json) {
    std::vector<TradingViewRowCandidate> rows;
    size_t pos = 0;
    while (true) {
        const size_t s_key = json.find("\"s\":\"", pos);
        if (s_key == std::string::npos) break;
        size_t sv = s_key + 5;
        size_t se = json.find('"', sv);
        if (se == std::string::npos) break;
        std::string symbol = json.substr(sv, se - sv);
        pos = se;
        auto colon = symbol.find(':');
        if (colon == std::string::npos) continue;

        TradingViewRowCandidate row;
        row.exchange = str::upper_trim_copy(symbol.substr(0, colon));
        row.symbol   = str::upper_trim_copy(symbol.substr(colon + 1));

        const size_t d_key = json.find("\"d\":[", pos);
        if (d_key == std::string::npos) break;
        size_t da = d_key + 5;
        size_t de = json.find(']', da);
        if (de == std::string::npos) break;
        std::string dvals = json.substr(da, de - da);
        pos = de;

        std::stringstream ss(dvals);
        std::string tok;
        std::vector<std::optional<double>> vals;
        while (std::getline(ss, tok, ',')) {
            tok = str::trim_copy(tok);
            if (tok == "null") vals.push_back(std::nullopt);
            else vals.push_back(parse_number_token(tok));
        }
        if (vals.size() >= 1) row.bid   = vals[0];
        if (vals.size() >= 2) row.ask   = vals[1];
        if (vals.size() >= 3) row.close = vals[2];
        rows.push_back(std::move(row));
    }
    return rows;
}

std::vector<CryptoQuote> MarketDataConnectors::build_tradingview_quotes(
    const std::vector<TradingViewRowCandidate>& rows,
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    double default_fee_bps,
    const std::vector<std::string>& requested_symbols,
    const std::vector<std::string>& allowed_exchanges)
{
    std::unordered_set<std::string> sym_set(
        requested_symbols.begin(), requested_symbols.end());
    std::unordered_set<std::string> ex_set(
        allowed_exchanges.begin(), allowed_exchanges.end());

    std::unordered_map<std::string, TradingViewRowCandidate> best;
    for (const auto& row : rows) {
        if (!sym_set.empty() && !sym_set.count(row.symbol))  continue;
        if (!ex_set.empty()  && !ex_set.count(row.exchange)) continue;
        const std::string key = row.exchange + ":" + row.symbol;
        auto it = best.find(key);
        const bool row_has_ba = row.bid.has_value() && row.ask.has_value();
        if (it == best.end()) { best[key] = row; continue; }
        const bool cur_has_ba = it->second.bid.has_value()
                             && it->second.ask.has_value();
        if (!cur_has_ba && row_has_ba) { it->second = row; continue; }
        if (!it->second.close.has_value() && row.close.has_value())
            it->second.close = row.close;
    }

    std::vector<CryptoQuote> out;
    for (auto& [key, row] : best) {
        double bid = 0.0, ask = 0.0;
        if (row.bid && row.ask) { bid = *row.bid; ask = *row.ask; }
        else if (row.close)     { bid = *row.close; ask = *row.close; }
        else continue;
        if (bid <= 0 || ask <= 0) continue;
        auto it = per_exchange_fee_bps.find(row.exchange);
        double fee = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
        out.push_back({row.exchange, row.symbol, bid, ask, fee});
    }
    return out;
}

std::string MarketDataConnectors::curl_http_get(const std::string& url) {
    const std::string cmd =
        "curl -sS -L --max-time 10 --user-agent 'ArbitrageMonitor/1.0' '"
        + url + "' 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw CsvError("popen failed: " + url);
    std::string r; char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) r += buf;
    pclose(pipe); return r;
}

std::string MarketDataConnectors::curl_http_post(const std::string& url,
                                                  const std::string& body) {
    const std::string cmd =
        "curl -sS -L --max-time 15 -X POST "
        "-H 'Content-Type: application/json' "
        "--user-agent 'ArbitrageMonitor/1.0' "
        "--data '" + body + "' '" + url + "' 2>&1";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) throw CsvError("popen POST failed: " + url);
    std::string r; char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) r += buf;
    pclose(pipe); return r;
}

std::string MarketDataConnectors::http_get(const std::string& url) const {
    return curl_http_get(url);
}

std::string MarketDataConnectors::http_post(const std::string& url,
                                             const std::string& body) const {
    return curl_http_post(url, body);
}

std::vector<CryptoQuote> MarketDataConnectors::fetch_btcusd_quotes(
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    double default_fee_bps) const
{
    auto fee = [&](const std::string& ex) {
        auto it = per_exchange_fee_bps.find(ex);
        return it != per_exchange_fee_bps.end() ? it->second : default_fee_bps;
    };
    std::vector<CryptoQuote> out;
    try {
        auto j = http_get("https://api.binance.com/api/v3/ticker/bookTicker?symbol=BTCUSDT");
        if (auto r = parse_binance_book_ticker(j))
            out.push_back({"BINANCE","BTCUSD",r->first,r->second,fee("BINANCE")});
    } catch (...) {}
    try {
        auto j = http_get("https://api.kraken.com/0/public/Ticker?pair=XBTUSD");
        if (auto r = parse_kraken_ticker(j))
            out.push_back({"KRAKEN","BTCUSD",r->first,r->second,fee("KRAKEN")});
    } catch (...) {}
    try {
        auto j = http_get("https://www.bitstamp.net/api/v2/ticker/btcusd/");
        if (auto r = parse_bitstamp_ticker(j))
            out.push_back({"BITSTAMP","BTCUSD",r->first,r->second,fee("BITSTAMP")});
    } catch (...) {}
    if (out.empty()) throw CsvError("All direct exchange fetches failed");
    return out;
}

std::vector<CryptoQuote> MarketDataConnectors::fetch_tradingview_quotes(
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    double default_fee_bps,
    const std::vector<std::string>& requested_symbols,
    const std::vector<std::string>& allowed_exchanges) const
{
    const std::string payload =
        R"({"columns":["bid","ask","close"],"sort":{"sortBy":"name","sortOrder":"asc"},"range":[0,500]})";
    std::vector<TradingViewRowCandidate> all_rows;
    for (const auto& market : {"crypto","cfd","forex"}) {
        try {
            const std::string url = std::string(
                "https://scanner.tradingview.com/") + market + "/scan";
            auto rows = parse_tradingview_scan_rows(http_post(url, payload));
            all_rows.insert(all_rows.end(), rows.begin(), rows.end());
        } catch (...) {}
    }
    auto out = build_tradingview_quotes(all_rows, per_exchange_fee_bps,
                                        default_fee_bps,
                                        requested_symbols, allowed_exchanges);
    if (out.empty())
        throw CsvError("Failed to fetch TradingView quotes for requested symbols");
    return out;
}

} // namespace am

namespace am {

static bool symbol_page_confirms_requested_ticker(
    const std::string& html,
    const std::string& exchange,
    const std::string& symbol)
{
    std::string upper_html = html;
    std::transform(upper_html.begin(), upper_html.end(), upper_html.begin(),
        [](unsigned char c){ return std::toupper(c); });
    const std::string ex  = str::upper_trim_copy(exchange);
    const std::string sym = str::upper_trim_copy(symbol);
    const std::string plain   = ex + ":" + sym;
    const std::string escaped = ex + "\\U003A" + sym;
    return upper_html.find(plain)   != std::string::npos
        || upper_html.find(escaped) != std::string::npos;
}

} // namespace am
