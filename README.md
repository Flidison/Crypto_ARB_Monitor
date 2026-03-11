# ArbitrageMonitor (crypto-only)

`ArbitrageMonitor` now runs only the crypto cross-exchange arbitrage monitor.
Project targets **C++23** and uses official **Google Test** for unit tests.

## C++23 justification
- Detailed rationale for required C++23/library features:
  - `docs/cpp23_rationale.md`

## Build
```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## Run once
```bash
./build/arbitrage_monitor run --config config/app.conf
```

## Continuous watch
```bash
./build/arbitrage_monitor watch --config config/app.conf --interval-sec 2
```

## Effective config
```bash
./build/arbitrage_monitor config --config config/app.conf
```

## Outputs
- `crypto_opportunities.csv`
  - file is recreated on process start
  - opportunities are appended with `observed_at` UTC timestamp each iteration
- `crypto_profit.csv` (when `profit_calc_enabled=true`)
  - file is recreated on process start
  - each iteration appends estimated profit from `start_capital` using best `net_pct`

## Offline and online modes
- Offline mode (`online_crypto_enabled=false`) reads `crypto_quotes_csv`.
- Online mode (`online_crypto_enabled=true`) pulls live quotes:
  - `crypto_online_source=TRADINGVIEW` for TradingView scanner + fallback pages
  - any other value uses direct APIs (Binance/Kraken/Bitstamp)

## Symbols
- `crypto_symbols` accepts CSV, e.g. `XTZUSD,APTUSD,ARBUSD`.
- If `crypto_symbols` is empty, monitor falls back to single `crypto_symbol`.

## Profit calculation
- Enable in config: `profit_calc_enabled=true`.
- Set base amount in config: `start_capital=1000`.
- Override at launch: `--start-capital 2500`.
- In `watch` mode capital compounds by iteration (`capital_after` becomes next `capital_before`).
