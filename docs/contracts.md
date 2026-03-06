# Data Contracts

## crypto_opportunities.csv

| Column         | Type   | Description                          |
|----------------|--------|--------------------------------------|
| observed_at    | string | UTC timestamp `YYYY-MM-DDTHH:MM:SSZ` |
| symbol         | string | Trading pair, e.g. `BTCUSD`          |
| buy_exchange   | string | Exchange to buy from                 |
| sell_exchange  | string | Exchange to sell on                  |
| buy_ask        | double | Ask price on buy exchange            |
| sell_bid       | double | Bid price on sell exchange           |
| gross_spread   | double | `sell_bid - buy_ask`                 |
| net_spread     | double | After fees                           |
| net_pct        | double | `net_spread / buy_cost`              |

## ingestion_report.csv (profit tracking)

| Column           | Type   | Description                       |
|------------------|--------|-----------------------------------|
| observed_at      | string | UTC timestamp                     |
| symbol           | string | Best opportunity symbol           |
| buy_exchange     | string |                                   |
| sell_exchange    | string |                                   |
| net_pct          | double | Best net percentage               |
| capital_before   | double | Capital before this iteration     |
| estimated_profit | double | `capital_before * net_pct`        |
| capital_after    | double | Becomes capital_before next round |

## Validation rules

- `observed_at` must match `YYYY-MM-DDTHH:MM:SSZ` exactly
- `bid > 0`, `ask > 0`, `ask >= bid`
- Relative paths resolved from config file directory
