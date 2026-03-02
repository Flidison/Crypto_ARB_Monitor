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

