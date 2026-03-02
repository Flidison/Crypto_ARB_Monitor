#include "online/MarketDataConnectors.h"
#include "exceptions/Exceptions.h"
#include "common/StringUtils.h"
#include <sstream>

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

std::string MarketDataConnectors::http_get(const std::string& url) const {
    return curl_http_get(url);
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

} // namespace am
