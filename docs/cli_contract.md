# CLI Contract

## Commands

```
arbitrage_monitor run    [--config <path>]
arbitrage_monitor watch  [--config <path>] [--interval-sec <n>]
arbitrage_monitor config [--config <path>]
```

## Options

- `--config <path>` — path to app.conf (default: `config/app.conf`)
- `--interval-sec <n>` — override `watch_interval_sec` from config
- Unknown `--` options are rejected with non-zero exit code

## Exit codes

- `0` — success
- `1` — error (config missing, bad option, runtime failure)
