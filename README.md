# Crypto Arbitrage Monitor

[![CI](https://github.com/Flidison/Crypto_ARB_Monitor/actions/workflows/ci.yml/badge.svg)](https://github.com/Flidison/Crypto_ARB_Monitor/actions/workflows/ci.yml)

A C++23 command-line application for monitoring cross-exchange cryptocurrency arbitrage opportunities.

Crypto Arbitrage Monitor loads or collects bid/ask quotes from multiple venues, normalizes them by symbol, and compares possible buy and sell legs across different venues. The detector accounts for configured fees and minimum net-spread thresholds, then writes qualifying observations to CSV.

The application supports one-shot and continuous monitoring, offline fixtures, TradingView-based collection, and a direct BTCUSD API mode. An optional projection estimates capital progression from the best detected net percentage. This project is an analytical monitor only: it does not place orders or execute trades.

## Key Features

- Cross-venue comparison for one or more configured symbols
- Offline quote ingestion from delimited text files with documented header aliases
- TradingView scanner collection with a verified symbol-page fallback
- Best-effort direct BTCUSD quotes from Binance, Kraken, and Bitstamp
- Per-quote, per-venue, or default fee inputs expressed in basis points
- Positive-spread, minimum net-spread, and minimum net-percentage filters
- One-shot `run`, continuous `watch`, and resolved `config` commands
- Opportunity and estimated-profit CSV reports with UTC observation timestamps
- Command-line overrides for runtime mode, fixture use, interval, and starting capital
- GoogleTest unit coverage plus deterministic CLI integration tests

## How It Works

```text
Offline CSV or online market data
                ↓
       Normalized venue quotes
                ↓
          Group by symbol
                ↓
 Compare buy/sell venue combinations
                ↓
       Apply fees and thresholds
                ↓
        Detected opportunities
                ↓
 CSV reporting and optional profit estimate
```

For each candidate pair, the engine adjusts the buy ask and sell bid by their respective fees. Only same-symbol, different-venue pairs with a positive net spread that meet both configured thresholds are retained. See the [data contract](docs/data_contract.md) for the formulas and field definitions.

## Project Structure

```text
Crypto_ARB_Monitor/
├── .github/workflows/ci.yml
├── config/
│   ├── app.conf
│   ├── fixture_crypto_quotes.csv
│   ├── offline_demo.conf
│   └── offline_demo_quotes.csv
├── docs/
│   ├── cli_contract.md
│   ├── cpp23_rationale.md
│   └── data_contract.md
├── external/googletest/CMakeLists.txt
├── include/
│   ├── common/
│   ├── config/
│   ├── crypto/
│   ├── exceptions/
│   └── online/
├── src/
│   ├── config/
│   ├── crypto/
│   ├── online/
│   └── main.cpp
├── tests/
├── .gitignore
├── CMakeLists.txt
└── README.md
```

Public interfaces live under `include/`; their implementations follow the same module names under `src/`. `config/` contains the default online configuration and deterministic offline data. `external/googletest/` contains only the CMake declaration that downloads GoogleTest 1.14.0 during test configuration.

## Requirements

- macOS or Linux
- A compiler with the C++23 library support required by [`std::string::contains`](docs/cpp23_rationale.md)
- CMake 3.20 or newer
- Ninja (optional, but used by CI and the commands below)
- `curl` on `PATH` for online collection modes
- Internet access for online quotes and the first test configuration, which downloads GoogleTest

The online connector currently invokes the `curl` executable through a POSIX process pipe. Windows is therefore not a supported runtime target in the current implementation.

## Build

Configure with tests enabled, build, and run the full suite:

```bash
cmake -S . -B build -G Ninja -DAM_BUILD_TESTS=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

Omit `-G Ninja` to use CMake's default generator. To build only the application, configure with `-DAM_BUILD_TESTS=OFF`; this also avoids downloading GoogleTest.

## Configuration

[`config/app.conf`](config/app.conf) is a flat `key = value` file. Relative input and output paths are resolved from the directory containing the selected config file.

```ini
crypto_symbols = XTZUSD,APTUSD,ARBUSD
crypto_exchanges = BINANCE,KRAKEN,BITSTAMP
crypto_default_fee_bps = 10.0
crypto_min_net_spread = 0.0
crypto_min_net_pct = 0.0
crypto_online_source = TRADINGVIEW
online_crypto_enabled = true
watch_interval_sec = 5
```

`crypto_symbols` takes precedence when it contains at least one value; otherwise the program falls back to `crypto_symbol`. Refer to the [CLI contract](docs/cli_contract.md) for every configuration key and override.

## Usage

Run one observation cycle:

```bash
./build/arbitrage_monitor run --config config/app.conf
```

Continue polling, with a two-second interval:

```bash
./build/arbitrage_monitor watch --config config/app.conf --interval-sec 2
```

Print the effective configuration, including resolved paths:

```bash
./build/arbitrage_monitor config --config config/app.conf
```

The CLI also supports `--online-crypto true|false`, `--no-fixtures true|false`, and `--start-capital <value>`. See [docs/cli_contract.md](docs/cli_contract.md) for command-specific details and exit behavior.

## Offline Example

The repository includes a deterministic three-venue snapshot and a matching configuration:

```bash
./build/arbitrage_monitor run --config config/offline_demo.conf
```

This command requires no market-data network access. It reads `config/offline_demo_quotes.csv`, detects the fixture's fee-adjusted XTZUSD opportunity, and writes the reports in the repository root.

## Output

`crypto_opportunities.csv` contains one row per detected venue pair:

```text
observed_at,symbol,buy_exchange,sell_exchange,buy_ask,sell_bid,gross_spread,net_spread,net_pct
2026-01-01T00:00:00Z,XTZUSD,BINANCE,KRAKEN,10.05000000,10.40000000,0.35000000,0.33977500,0.03379156
```

When `profit_calc_enabled=true`, `crypto_profit.csv` records the starting capital, selected best opportunity, estimated profit, and projected capital after each iteration. Both reports are reset when the process starts and appended on each `run` or `watch` iteration. They are runtime artifacts and are intentionally ignored by Git.

The estimate is hypothetical, not realized PnL. Complete schemas and calculation rules are in [docs/data_contract.md](docs/data_contract.md).

## Testing

The `am_tests` executable covers configuration parsing, CSV ingestion, fee-adjusted opportunity detection, profit reporting, direct API payload parsing, and TradingView normalization/fallback behavior. CTest also runs deterministic CLI end-to-end and argument/path-resolution checks.

GitHub Actions uses the same Ninja configure, build, and CTest sequence on Ubuntu for pushes and pull requests.

## Documentation

- [CLI and configuration contract](docs/cli_contract.md)
- [Input, output, and calculation contract](docs/data_contract.md)
- [C++23 rationale](docs/cpp23_rationale.md)

## Contributors

- **Gareev Amirkhan ([Flidison](https://github.com/Flidison))** — CMake and project setup, configuration and arbitrage-engine work, calculations and CSV reporting, automated tests, documentation, and repository maintenance.
- **Ryadinskiy Konstantin** — CLI orchestration, `run`/`watch`/`config` modes, path resolution, runtime source selection, profit-flow integration, CLI tests, and supporting documentation.
- **Timur Rozovel** — online market-data connectors for Binance, Kraken, Bitstamp, and TradingView; HTTP transport, response parsing, fallback behavior, and related tests and fixtures.

Responsibilities are summarized from the repository's [commit history](https://github.com/Flidison/Crypto_ARB_Monitor/commits/main/) and [contributor graph](https://github.com/Flidison/Crypto_ARB_Monitor/graphs/contributors).

## Limitations

- The monitor does not execute trades, manage exchange accounts, or transfer funds.
- Quote comparison does not model order-book depth, slippage, latency, transfer time, funding state, withdrawal costs, or venue-specific execution constraints.
- TradingView symbol-page fallback uses a close price as a synthetic bid and ask when level-one data is unavailable; it should not be treated as an executable quote.
- The direct API mode is limited to BTCUSD-equivalent tickers on Binance, Kraken, and Bitstamp.
- Online collection depends on external response formats, network availability, and a local `curl` executable.
- Estimated capital progression assumes the best observed net percentage can be applied to the full configured capital and, in watch mode, compounded each iteration.

An observed spread is therefore a signal for further analysis, not evidence of a realizable or guaranteed profit.

## Disclaimer

This project is for research, educational, and analytical purposes only. It is not financial advice.
