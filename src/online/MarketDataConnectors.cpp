#include "online/MarketDataConnectors.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <memory>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include "common/StringUtils.h"
#include "exceptions/Exceptions.h"

namespace am {

namespace {

static std::optional<double> parse_number_token(const std::string& token) {
    const std::string t = str::trim_copy(token);
    if (t.empty()) return std::nullopt;
    char* end = nullptr;
    const double v = std::strtod(t.c_str(), &end);
    if (end == t.c_str() || *end != '\0' || !std::isfinite(v)) return std::nullopt;
    return v;
}

static std::optional<std::string> find_value_after_key(const std::string& json, const std::string& key) {
    const std::string k = "\"" + key + "\"";
    const size_t p = json.find(k);
    if (p == std::string::npos) return std::nullopt;
    size_t c = json.find(':', p + k.size());
    if (c == std::string::npos) return std::nullopt;
    ++c;
    while (c < json.size() && std::isspace(static_cast<unsigned char>(json[c]))) ++c;
    if (c >= json.size()) return std::nullopt;

    if (json[c] == '"') {
        ++c;
        const size_t e = json.find('"', c);
        if (e == std::string::npos) return std::nullopt;
        return json.substr(c, e - c);
    }

    size_t e = c;
    while (e < json.size() && json[e] != ',' && json[e] != '}' && json[e] != ']' && !std::isspace(static_cast<unsigned char>(json[e]))) ++e;
    return json.substr(c, e - c);
}

static std::optional<std::string> first_string_in_array_after_key(const std::string& json, const std::string& key) {
    const std::string k = "\"" + key + "\"";
    const size_t p = json.find(k);
    if (p == std::string::npos) return std::nullopt;
    size_t a = json.find('[', p + k.size());
    if (a == std::string::npos) return std::nullopt;
    ++a;
    while (a < json.size() && std::isspace(static_cast<unsigned char>(json[a]))) ++a;
    if (a >= json.size() || json[a] != '"') return std::nullopt;
    ++a;
    const size_t e = json.find('"', a);
    if (e == std::string::npos) return std::nullopt;
    return json.substr(a, e - a);
}

static std::vector<std::string> split_json_array_tokens(const std::string& s) {
    std::vector<std::string> out;
    std::string cell;
    bool in_quotes = false;
    bool escaped = false;
    for (char ch : s) {
        if (escaped) {
            cell.push_back(ch);
            escaped = false;
            continue;
        }
        if (ch == '\\') {
            cell.push_back(ch);
            escaped = true;
            continue;
        }
        if (ch == '"') {
            in_quotes = !in_quotes;
            cell.push_back(ch);
            continue;
        }
        if (ch == ',' && !in_quotes) {
            out.push_back(str::trim_copy(cell));
            cell.clear();
            continue;
        }
        cell.push_back(ch);
    }
    out.push_back(str::trim_copy(cell));
    return out;
}

static std::string unquote_json_token(const std::string& token) {
    std::string t = str::trim_copy(token);
    if (t.size() < 2 || t.front() != '"' || t.back() != '"') return t;
    t = t.substr(1, t.size() - 2);
    std::string out;
    out.reserve(t.size());
    bool esc = false;
    for (char ch : t) {
        if (esc) {
            out.push_back(ch);
            esc = false;
        } else if (ch == '\\') {
            esc = true;
        } else {
            out.push_back(ch);
        }
    }
    return out;
}

static std::vector<TradingViewRowCandidate> parse_tradingview_scan_rows(const std::string& json) {
    // The scanner parser intentionally handles only "s" and "d[2..4]" to avoid a JSON dependency.
    std::vector<TradingViewRowCandidate> rows;
    size_t pos = 0;
    while (true) {
        const size_t s_key = json.find("\"s\":\"", pos);
        if (s_key == std::string::npos) break;
        const size_t s_val_start = s_key + 5;
        const size_t s_val_end = json.find('"', s_val_start);
        if (s_val_end == std::string::npos) break;
        const std::string symbol = json.substr(s_val_start, s_val_end - s_val_start);

        const size_t d_key = json.find("\"d\":[", s_val_end);
        if (d_key == std::string::npos) break;
        size_t arr_start = json.find('[', d_key);
        if (arr_start == std::string::npos) break;
        ++arr_start;

        bool in_quotes = false;
        bool escaped = false;
        int depth = 1;
        size_t i = arr_start;
        for (; i < json.size(); ++i) {
            const char ch = json[i];
            if (escaped) {
                escaped = false;
                continue;
            }
            if (ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') {
                in_quotes = !in_quotes;
                continue;
            }
            if (!in_quotes) {
                if (ch == '[') ++depth;
                else if (ch == ']') {
                    --depth;
                    if (depth == 0) break;
                }
            }
        }
        if (i >= json.size()) break;

        const std::string data_arr = json.substr(arr_start, i - arr_start);
        pos = i + 1;

        TradingViewRowCandidate row;
        const auto colon = symbol.find(':');
        if (colon == std::string::npos) {
            row.exchange = str::upper_trim_copy(symbol);
            row.symbol = "";
        } else {
            row.exchange = str::upper_trim_copy(symbol.substr(0, colon));
            row.symbol = str::upper_trim_copy(symbol.substr(colon + 1));
        }

        const auto tokens = split_json_array_tokens(data_arr);
        if (tokens.size() >= 5) {
            row.bid = parse_number_token(unquote_json_token(tokens[2]));
            row.ask = parse_number_token(unquote_json_token(tokens[3]));
            row.close = parse_number_token(unquote_json_token(tokens[4]));
        }
        rows.push_back(std::move(row));
    }
    return rows;
}

static std::string parse_tv_price_from_symbol_page(const std::string& html) {
    const std::string key = "current price of ";
    const size_t p = html.find(key);
    if (p == std::string::npos) return "";
    size_t s = html.find(" is ", p + key.size());
    if (s == std::string::npos) return "";
    s += 4;
    size_t e = html.find("USD", s);
    if (e == std::string::npos || e <= s) return "";
    std::string raw = html.substr(s, e - s);
    raw = str::trim_copy(raw);
    std::string out;
    out.reserve(raw.size());
    for (char ch : raw) {
        if ((ch >= '0' && ch <= '9') || ch == '.') out.push_back(ch);
    }
    return out;
}

static bool symbol_page_confirms_requested_ticker(
    const std::string& html,
    const std::string& exchange,
    const std::string& symbol) {

    const std::string ex = str::upper_trim_copy(exchange);
    const std::string sym = str::upper_trim_copy(symbol);
    if (ex.empty() || sym.empty() || html.empty()) return false;

    const std::string upper_html = str::upper_copy(html);
    const std::string plain = ex + ":" + sym;
    if (upper_html.find(plain) != std::string::npos) return true;

    // TradingView may encode ':' as \u003A in embedded symbol identifiers.
    const std::string escaped = ex + "\\U003A" + sym;
    return upper_html.find(escaped) != std::string::npos;
}

class PipeReader {
public:
    explicit PipeReader(const std::string& cmd)
        : pipe_(::popen(cmd.c_str(), "r"), &::pclose) {
        if (!pipe_) throw CsvError("Failed to execute curl command");
    }

    FILE* get() const { return pipe_.get(); }

    int close() {
        // release() prevents the unique_ptr deleter from calling pclose a second time.
        FILE* raw = pipe_.release();
        if (!raw) return 0;
        return ::pclose(raw);
    }

private:
    std::unique_ptr<FILE, int (*)(FILE*)> pipe_;
};

} // namespace

MarketDataConnectors::MarketDataConnectors()
    : http_get_fn_(curl_http_get),
      http_post_json_fn_(curl_http_post_json) {}

MarketDataConnectors::MarketDataConnectors(HttpGetFn http_get_fn, HttpPostJsonFn http_post_json_fn)
    : http_get_fn_(std::move(http_get_fn)),
      http_post_json_fn_(std::move(http_post_json_fn)) {
    if (!http_get_fn_ || !http_post_json_fn_) {
        throw DataValidationError("MarketDataConnectors requires non-empty HTTP handlers");
    }
}

std::string MarketDataConnectors::shell_escape_single_quotes(const std::string& s) {
    std::string out;
    out.reserve(s.size() + 8);
    for (char ch : s) {
        if (ch == '\'') out += "'\"'\"'";
        else out.push_back(ch);
    }
    return out;
}

std::optional<double> MarketDataConnectors::extract_json_number_field(const std::string& json, const std::string& field) {
    const auto v = find_value_after_key(json, field);
    if (!v.has_value()) return std::nullopt;
    return parse_number_token(*v);
}

std::optional<std::pair<double, double>> MarketDataConnectors::parse_binance_book_ticker(const std::string& json) {
    const auto bid = extract_json_number_field(json, "bidPrice");
    const auto ask = extract_json_number_field(json, "askPrice");
    if (!bid.has_value() || !ask.has_value()) return std::nullopt;
    if (*bid <= 0.0 || *ask <= 0.0 || *ask < *bid) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::optional<std::pair<double, double>> MarketDataConnectors::parse_kraken_ticker(const std::string& json) {
    const auto b = first_string_in_array_after_key(json, "b");
    const auto a = first_string_in_array_after_key(json, "a");
    if (!b.has_value() || !a.has_value()) return std::nullopt;
    const auto bid = parse_number_token(*b);
    const auto ask = parse_number_token(*a);
    if (!bid.has_value() || !ask.has_value()) return std::nullopt;
    if (*bid <= 0.0 || *ask <= 0.0 || *ask < *bid) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::optional<std::pair<double, double>> MarketDataConnectors::parse_bitstamp_ticker(const std::string& json) {
    const auto bid = extract_json_number_field(json, "bid");
    const auto ask = extract_json_number_field(json, "ask");
    if (!bid.has_value() || !ask.has_value()) return std::nullopt;
    if (*bid <= 0.0 || *ask <= 0.0 || *ask < *bid) return std::nullopt;
    return std::make_pair(*bid, *ask);
}

std::vector<CryptoQuote> MarketDataConnectors::build_tradingview_quotes(
    const std::vector<TradingViewRowCandidate>& rows,
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    const double default_fee_bps,
    const std::vector<std::string>& symbols,
    const std::unordered_set<std::string>& allowed_exchanges) {

    if (symbols.empty()) return {};

    std::unordered_set<std::string> selected_symbols;
    selected_symbols.reserve(symbols.size());
    for (const auto& raw_symbol : symbols) {
        const std::string symbol = str::upper_trim_copy(raw_symbol);
        if (!symbol.empty()) selected_symbols.insert(symbol);
    }
    if (selected_symbols.empty()) return {};

    std::unordered_set<std::string> exchange_filter;
    exchange_filter.reserve(allowed_exchanges.size());
    for (const auto& raw_exchange : allowed_exchanges) {
        const std::string exchange = str::upper_trim_copy(raw_exchange);
        if (!exchange.empty()) exchange_filter.insert(exchange);
    }

    std::unordered_map<std::string, TradingViewRowCandidate> merged;
    for (const auto& raw_row : rows) {
        TradingViewRowCandidate row = raw_row;
        row.exchange = str::upper_trim_copy(row.exchange);
        row.symbol = str::upper_trim_copy(row.symbol);
        if (row.exchange.empty() || row.symbol.empty()) continue;
        if (selected_symbols.count(row.symbol) == 0) continue;
        if (!exchange_filter.empty() && exchange_filter.count(row.exchange) == 0) continue;

        const std::string key = row.exchange + "|" + row.symbol;
        auto& cur = merged[key];
        if (cur.exchange.empty()) {
            cur = std::move(row);
            continue;
        }

        const bool cur_has_bid_ask = cur.bid.has_value() && cur.ask.has_value();
        const bool row_has_bid_ask = row.bid.has_value() && row.ask.has_value();
        if (!cur_has_bid_ask && row_has_bid_ask) {
            cur = std::move(row);
            continue;
        }

        if (!cur.close.has_value() && row.close.has_value()) {
            cur.close = row.close;
        }
    }

    std::vector<CryptoQuote> out;
    out.reserve(merged.size());
    for (const auto& [key, row] : merged) {
        (void)key;
        double bid = row.bid.value_or(0.0);
        double ask = row.ask.value_or(0.0);
        const double close = row.close.value_or(0.0);
        if (bid <= 0.0 || ask <= 0.0 || ask < bid) {
            if (close > 0.0) {
                bid = close;
                ask = close;
            }
        }
        if (bid <= 0.0 || ask <= 0.0 || ask < bid) continue;

        const auto it = per_exchange_fee_bps.find(row.exchange);
        const double fee = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
        out.push_back(CryptoQuote{row.exchange, row.symbol, bid, ask, fee});
    }

    std::sort(out.begin(), out.end(), [](const CryptoQuote& a, const CryptoQuote& b) {
        if (a.exchange != b.exchange) return a.exchange < b.exchange;
        return a.symbol < b.symbol;
    });
    return out;
}

std::string MarketDataConnectors::curl_http_get(const std::string& url) {
    const std::string cmd =
        "curl -sS -L -w '\\n__HTTP_STATUS__:%{http_code}' '" + shell_escape_single_quotes(url) + "' 2>/dev/null";
    PipeReader pipe(cmd);

    std::string out;
    char buf[4096];
    while (true) {
        const size_t n = std::fread(buf, 1, sizeof(buf), pipe.get());
        if (n > 0) out.append(buf, n);
        if (n < sizeof(buf)) break;
    }
    const int rc = pipe.close();
    if (rc != 0 || out.empty()) throw CsvError("HTTP GET failed for url: " + url);

    // curl's process status does not expose HTTP failures, so append a response-code marker.
    const std::string marker = "__HTTP_STATUS__:";
    const size_t m = out.rfind(marker);
    if (m == std::string::npos) {
        throw CsvError("HTTP GET failed: status marker missing for url: " + url);
    }

    std::string body = out.substr(0, m);
    std::string status_s = str::trim_copy(out.substr(m + marker.size()));
    const auto status = parse_number_token(status_s);
    if (!status.has_value()) {
        throw CsvError("HTTP GET failed: bad status for url: " + url);
    }
    const int code = static_cast<int>(*status);

    while (!body.empty() && (body.back() == '\r' || body.back() == '\n')) body.pop_back();
    if (code < 200 || code >= 300) {
        std::string snippet = body.substr(0, std::min<size_t>(200, body.size()));
        throw CsvError("HTTP GET failed (status " + std::to_string(code) + ") for url: " + url +
                       ", response: " + snippet);
    }
    return body;
}

std::string MarketDataConnectors::curl_http_post_json(const std::string& url, const std::string& json_body) {
    const std::string cmd =
        "curl -sS -L -X POST -H 'content-type: application/json' --data '" +
        shell_escape_single_quotes(json_body) + "' -w '\\n__HTTP_STATUS__:%{http_code}' '" +
        shell_escape_single_quotes(url) + "' 2>/dev/null";
    PipeReader pipe(cmd);

    std::string out;
    char buf[4096];
    while (true) {
        const size_t n = std::fread(buf, 1, sizeof(buf), pipe.get());
        if (n > 0) out.append(buf, n);
        if (n < sizeof(buf)) break;
    }
    const int rc = pipe.close();
    if (rc != 0 || out.empty()) throw CsvError("HTTP POST failed for url: " + url);

    const std::string marker = "__HTTP_STATUS__:";
    const size_t m = out.rfind(marker);
    if (m == std::string::npos) throw CsvError("HTTP POST failed: status marker missing for url: " + url);

    std::string body = out.substr(0, m);
    std::string status_s = str::trim_copy(out.substr(m + marker.size()));
    const auto status = parse_number_token(status_s);
    if (!status.has_value()) throw CsvError("HTTP POST failed: bad status for url: " + url);
    const int code = static_cast<int>(*status);

    while (!body.empty() && (body.back() == '\r' || body.back() == '\n')) body.pop_back();
    if (code < 200 || code >= 300) {
        std::string snippet = body.substr(0, std::min<size_t>(200, body.size()));
        throw CsvError("HTTP POST failed (status " + std::to_string(code) + ") for url: " + url +
                       ", response: " + snippet);
    }
    return body;
}

std::string MarketDataConnectors::http_get(const std::string& url) const {
    return http_get_fn_(url);
}

std::string MarketDataConnectors::http_post_json(const std::string& url, const std::string& json_body) const {
    return http_post_json_fn_(url, json_body);
}

std::vector<CryptoQuote> MarketDataConnectors::fetch_btcusd_quotes(
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    const double default_fee_bps) const {

    std::vector<CryptoQuote> out;

    // Direct venues are best-effort; the request fails only if none returns a usable quote.
    try {
        const auto json = http_get("https://api.binance.com/api/v3/ticker/bookTicker?symbol=BTCUSDT");
        const auto ba = parse_binance_book_ticker(json);
        if (ba.has_value()) {
            const auto it = per_exchange_fee_bps.find("BINANCE");
            const double fee = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
            out.push_back(CryptoQuote{"BINANCE", "BTCUSD", ba->first, ba->second, fee});
        }
    } catch (...) {
    }

    try {
        const auto json = http_get("https://api.kraken.com/0/public/Ticker?pair=XBTUSD");
        const auto ba = parse_kraken_ticker(json);
        if (ba.has_value()) {
            const auto it = per_exchange_fee_bps.find("KRAKEN");
            const double fee = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
            out.push_back(CryptoQuote{"KRAKEN", "BTCUSD", ba->first, ba->second, fee});
        }
    } catch (...) {
    }

    try {
        const auto json = http_get("https://www.bitstamp.net/api/v2/ticker/btcusd/");
        const auto ba = parse_bitstamp_ticker(json);
        if (ba.has_value()) {
            const auto it = per_exchange_fee_bps.find("BITSTAMP");
            const double fee = (it != per_exchange_fee_bps.end()) ? it->second : default_fee_bps;
            out.push_back(CryptoQuote{"BITSTAMP", "BTCUSD", ba->first, ba->second, fee});
        }
    } catch (...) {
    }

    if (out.empty()) {
        throw CsvError("Failed to fetch BTCUSD quotes from all configured exchanges");
    }

    return out;
}

std::vector<CryptoQuote> MarketDataConnectors::fetch_tradingview_quotes(
    const std::unordered_map<std::string, double>& per_exchange_fee_bps,
    const double default_fee_bps,
    const std::vector<std::string>& requested_symbols,
    const std::unordered_set<std::string>& allowed_exchanges) const {

    const std::vector<std::string> all_exchanges = {
        "BINANCE", "KRAKEN", "BITSTAMP", "FXPRO", "OANDA", "PEPPERSTONE",
    };
    if (requested_symbols.empty()) throw CsvError("No symbols selected for TradingView online mode");

    std::unordered_map<std::string, std::pair<std::string, std::string>> key_to_pair;
    std::vector<std::string> tickers_list;
    tickers_list.reserve(all_exchanges.size() * requested_symbols.size());
    for (const auto& ex : all_exchanges) {
        if (!allowed_exchanges.empty() && allowed_exchanges.count(ex) == 0) continue;
        for (const auto& sym : requested_symbols) {
            const std::string key = ex + "|" + sym;
            key_to_pair[key] = std::make_pair(ex, sym);
            tickers_list.push_back(ex + ":" + sym);
        }
    }
    if (tickers_list.empty()) throw CsvError("No exchanges selected for TradingView online mode");

    std::stringstream tickers;
    for (size_t i = 0; i < tickers_list.size(); ++i) {
        if (i) tickers << ",";
        tickers << "\"" << tickers_list[i] << "\"";
    }
    const std::string body =
        "{\"symbols\":{\"tickers\":[" + tickers.str() +
        "],\"query\":{\"types\":[]}},\"columns\":[\"name\",\"exchange\",\"bid\",\"ask\",\"close\",\"description\"]}";

    auto fetch_market = [&](const std::string& market) -> std::vector<TradingViewRowCandidate> {
        const auto json = http_post_json("https://scanner.tradingview.com/" + market + "/scan", body);
        return parse_tradingview_scan_rows(json);
    };

    // TradingView classifies requested tickers across independent scanner markets.
    auto f_crypto = std::async(std::launch::async, [&]() { return fetch_market("crypto"); });
    auto f_cfd = std::async(std::launch::async, [&]() { return fetch_market("cfd"); });
    auto f_forex = std::async(std::launch::async, [&]() { return fetch_market("forex"); });

    std::vector<TradingViewRowCandidate> all_rows;
    auto append_rows = [&](std::vector<TradingViewRowCandidate> rows) {
        all_rows.reserve(all_rows.size() + rows.size());
        for (auto& row : rows) all_rows.push_back(std::move(row));
    };

    try { append_rows(f_crypto.get()); } catch (...) {}
    try { append_rows(f_cfd.get()); } catch (...) {}
    try { append_rows(f_forex.get()); } catch (...) {}

    std::unordered_set<std::string> covered_keys;
    for (const auto& row : all_rows) {
        const std::string ex = str::upper_trim_copy(row.exchange);
        const std::string sym = str::upper_trim_copy(row.symbol);
        if (ex.empty() || sym.empty()) continue;
        const std::string key = ex + "|" + sym;
        if (key_to_pair.count(key) == 0) continue;
        const bool has_bid_ask = row.bid.has_value() && row.ask.has_value() && *row.bid > 0.0 && *row.ask > 0.0 && *row.ask >= *row.bid;
        const bool has_close = row.close.has_value() && *row.close > 0.0;
        if (has_bid_ask || has_close) covered_keys.insert(key);
    }

    // Scanner gaps fall back to verified symbol pages, which provide close-only quotes.
    for (const auto& kv : key_to_pair) {
        const std::string& key = kv.first;
        const std::string& ex = kv.second.first;
        const std::string& sym = kv.second.second;
        if (covered_keys.count(key) > 0) continue;
        try {
            const std::string html = http_get("https://www.tradingview.com/symbols/" + sym + "/?exchange=" + ex);
            if (!symbol_page_confirms_requested_ticker(html, ex, sym)) continue;
            const std::string p = parse_tv_price_from_symbol_page(html);
            const auto mid = parse_number_token(p);
            if (mid.has_value() && *mid > 0.0) {
                TradingViewRowCandidate r;
                r.exchange = ex;
                r.symbol = sym;
                r.close = *mid;
                all_rows.push_back(std::move(r));
            }
        } catch (...) {
        }
    }

    const auto out = build_tradingview_quotes(
        all_rows, per_exchange_fee_bps, default_fee_bps, requested_symbols, allowed_exchanges);

    if (out.empty()) {
        throw CsvError("Failed to fetch TradingView quotes for requested symbols (scanner/page parsing)");
    }
    return out;
}

} // namespace am
