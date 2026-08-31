# CLI and Configuration Contract

The executable is named `arbitrage_monitor`. With no command, it behaves as `run` and loads `config/app.conf` relative to the current working directory.

## Commands

```text
arbitrage_monitor run [options]
arbitrage_monitor watch [options]
arbitrage_monitor config [options]
```

| Command | Behavior |
| --- | --- |
| `run` | Fetch or load quotes, detect opportunities, write reports, and exit. |
| `watch` | Repeat the `run` iteration indefinitely. A failed iteration is logged and does not stop the watcher. |
| `config` | Load configuration, apply overrides, resolve paths, print effective values, and exit without collecting quotes. |

Any other command prints usage and returns exit code `1`.

## Options

| Option | Accepted value | Behavior |
| --- | --- | --- |
| `--config` | Path | Selects the config file; default is `config/app.conf`. |
| `--interval-sec` | Integer | Overrides the watch interval. `watch` sleeps for at least one second. |
| `--online-crypto` | `true`, `false`, `1`, or `0` | Overrides online/offline mode; matching is case-insensitive. |
| `--no-fixtures` | `true`, `false`, `1`, or `0` | Clears quote/fee paths recognized as fixture or test paths. |
| `--start-capital` | Number | Overrides the estimated-profit starting capital; negative values are rejected. |

Unknown options, missing option values, and positional arguments are rejected rather than ignored.

## Configuration Format

Configuration uses one `key = value` entry per line. Empty lines and lines beginning with `#` are ignored. Unknown keys are currently ignored. Boolean config values accept lowercase `true`, `false`, `1`, or `0`.

| Key | Default | Purpose |
| --- | --- | --- |
| `crypto_quotes_csv` | Empty | Offline quote input. Required in offline mode. |
| `crypto_fees_csv` | Empty | Optional per-exchange fee input. |
| `crypto_output_csv` | `crypto_opportunities.csv` | Opportunity report path. |
| `profit_output_csv` | `crypto_profit.csv` | Estimated-profit report path. |
| `crypto_symbol` | `BTCUSD` | Single-symbol fallback. |
| `crypto_symbols` | Value of `crypto_symbol` | Comma-separated symbol list. |
| `crypto_exchanges` | Six built-in venue names | Comma-separated allowed venue list. |
| `crypto_default_fee_bps` | `10.0` | Fee used when no quote or exchange fee is available. |
| `crypto_min_net_spread` | `0.0` | Minimum fee-adjusted spread per unit. |
| `crypto_min_net_pct` | `0.0` | Minimum fee-adjusted fractional return. |
| `crypto_online_source` | `TRADINGVIEW` | `TRADINGVIEW` selects scanner/page collection; any other value selects direct APIs. |
| `online_crypto_enabled` | `false` | Selects online collection instead of offline CSV. |
| `watch_interval_sec` | `5` | Poll interval used by `watch`. |
| `profit_calc_enabled` | `false` | Enables the estimated-profit report. |
| `start_capital` | `1000.0` | Initial amount used by the estimate. |
| `no_fixtures` | `false` | Disables recognized fixture/test file paths. |

The direct API source returns BTCUSD quotes only. If the configured symbol selection excludes BTCUSD, those quotes are filtered out before opportunity detection.

## Path Resolution

Relative values for `crypto_quotes_csv`, `crypto_fees_csv`, `crypto_output_csv`, and `profit_output_csv` are resolved against the directory containing the selected config file. `config` prints the resulting absolute or normalized paths.

## Reports

Before the first `run` or `watch` iteration, the opportunity report is recreated. The estimated-profit report is also recreated when enabled. Watch iterations append to those initialized files. See the [data contract](data_contract.md) for schemas and formulas.

## Exit Behavior

| Code | Meaning |
| --- | --- |
| `0` | Successful `run` or `config` completion. |
| `1` | Unknown command; usage was printed. |
| `2` | Configuration, CSV, or data-validation error derived from `AmException`. |
| `3` | Other standard exception. |

`watch` catches per-iteration exceptions and continues, so these process exit codes primarily apply to startup, parsing, `run`, and `config` failures.
