# Crypto Data Contract

This document defines the delimited input formats, opportunity calculation, and generated reports used by Crypto Arbitrage Monitor.

## Quote Input

`crypto_quotes_csv` points to the offline quote file. The parser selects the most frequent supported delimiter in the header: comma, semicolon, tab, or pipe. Header matching is case-insensitive, surrounding cell whitespace is trimmed, and quoted fields may escape a quote as `""`.

| Logical field | Accepted headers | Required | Validation |
| --- | --- | --- | --- |
| Exchange | `exchange`, `venue`, `broker` | Yes | Normalized to uppercase |
| Symbol | `symbol`, `ticker`, `pair` | Yes | Normalized to uppercase |
| Bid | `bid`, `bid_price` | Yes | Finite and greater than zero |
| Ask | `ask`, `ask_price` | Yes | Finite, greater than zero, and not below bid |
| Fee | `fee_bps`, `commission_bps` | No | Finite numeric value |

Each non-empty data row must contain the same number of fields as the header. Multiline quoted fields are not supported.

## Fee Input and Precedence

`crypto_fees_csv` is optional. When present, it requires an exchange column (`exchange`, `venue`, or `broker`) and a fee column (`fee_bps` or `commission_bps`). Exchange names are normalized to uppercase; a later duplicate exchange row replaces an earlier value.

The fee for an offline quote is selected in this order:

1. The quote row's `fee_bps` value, when that column exists.
2. The matching exchange value from `crypto_fees_csv`.
3. `crypto_default_fee_bps`.

Online quotes use the per-exchange fee file when available and otherwise use `crypto_default_fee_bps`.

## Opportunity Calculation

The engine evaluates ordered quote pairs after symbol and allowed-exchange filtering. A buy and sell leg must use the same symbol and different exchange names.

For each candidate pair:

```text
buy_cost      = buy_ask × (1 + buy_fee_bps / 10,000)
sell_proceeds = sell_bid × (1 - sell_fee_bps / 10,000)
gross_spread = sell_bid - buy_ask
net_spread   = sell_proceeds - buy_cost
net_pct      = net_spread / buy_cost
```

A candidate is retained only when all of the following are true:

- `net_spread > 0`
- `net_spread >= crypto_min_net_spread`
- `net_pct >= crypto_min_net_pct`

Results are sorted by descending `net_spread`.

## Opportunities Output

`crypto_output_csv` defaults to `crypto_opportunities.csv` and has these columns:

| Column | Meaning |
| --- | --- |
| `observed_at` | UTC timestamp in `YYYY-MM-DDTHH:MM:SSZ` form |
| `symbol` | Normalized symbol shared by both legs |
| `buy_exchange` | Venue providing the buy ask |
| `sell_exchange` | Venue providing the sell bid |
| `buy_ask` | Unadjusted ask from the buy venue |
| `sell_bid` | Unadjusted bid from the sell venue |
| `gross_spread` | `sell_bid - buy_ask` |
| `net_spread` | Fee-adjusted spread per unit |
| `net_pct` | Fee-adjusted spread divided by fee-adjusted buy cost |

The file is truncated and its header is written once when the process starts. Each iteration then appends its opportunity rows. An iteration with no opportunities adds no data row.

## Estimated-Profit Output

When `profit_calc_enabled=true`, `profit_output_csv` defaults to `crypto_profit.csv` and has these columns:

| Column | Meaning |
| --- | --- |
| `observed_at` | UTC observation timestamp |
| `capital_before` | Capital supplied to the current iteration |
| `opportunities_count` | Number of retained opportunities |
| `best_symbol` | Symbol of the highest-`net_pct` opportunity |
| `buy_exchange` | Buy venue for that opportunity |
| `sell_exchange` | Sell venue for that opportunity |
| `best_net_pct` | Highest retained `net_pct` |
| `estimated_profit` | `capital_before × best_net_pct` |
| `capital_after` | `capital_before + estimated_profit` |

If no opportunity is found, the venue/symbol fields are empty, the percentages and estimated profit are zero, and capital is unchanged. In `watch` mode, `capital_after` becomes the next iteration's `capital_before`. The file is reset at process start and receives one row per iteration.

This calculation is a projection only. It assumes the full capital can capture the displayed percentage and does not account for market depth, slippage, latency, transfer costs, or execution constraints.
