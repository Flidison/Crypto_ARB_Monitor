if(NOT DEFINED REPO_ROOT OR NOT DEFINED WORKDIR OR NOT DEFINED MONITOR_BIN)
  message(FATAL_ERROR "REPO_ROOT, WORKDIR and MONITOR_BIN must be defined")
endif()

set(E2E_DIR "${WORKDIR}/cli_e2e")
file(REMOVE_RECURSE "${E2E_DIR}")
file(MAKE_DIRECTORY "${E2E_DIR}")

set(QUOTES_FILE "${E2E_DIR}/quotes.csv")
set(CONFIG_FILE "${E2E_DIR}/app.conf")
set(OUT_FILE "${E2E_DIR}/crypto_opportunities.csv")

file(WRITE "${QUOTES_FILE}"
  # Deterministic offline market snapshot with at least one profitable pair.
  "exchange,symbol,bid,ask,fee_bps\n"
  "BINANCE,XTZUSD,10.00,10.05,5\n"
  "KRAKEN,XTZUSD,10.40,10.45,5\n"
  "BITSTAMP,XTZUSD,10.20,10.25,5\n"
)

file(WRITE "${CONFIG_FILE}"
  # Explicit test config so the E2E check does not depend on repository defaults.
  "crypto_quotes_csv = quotes.csv\n"
  "crypto_fees_csv =\n"
  "crypto_output_csv = crypto_opportunities.csv\n"
  "profit_output_csv = crypto_profit.csv\n"
  "crypto_symbol = XTZUSD\n"
  "crypto_symbols = XTZUSD\n"
  "crypto_exchanges = BINANCE,KRAKEN,BITSTAMP\n"
  "crypto_default_fee_bps = 10.0\n"
  "crypto_min_net_spread = 0.0\n"
  "crypto_min_net_pct = 0.0\n"
  "crypto_online_source = TRADINGVIEW\n"
  "online_crypto_enabled = false\n"
  "watch_interval_sec = 1\n"
  "profit_calc_enabled = false\n"
  "start_capital = 1000\n"
)

file(REMOVE "${OUT_FILE}")

execute_process(
  COMMAND "${MONITOR_BIN}" run --config "${CONFIG_FILE}" --online-crypto false
  WORKING_DIRECTORY "${E2E_DIR}"
  RESULT_VARIABLE run_rc
  OUTPUT_VARIABLE run_out
  ERROR_VARIABLE run_err
)
if(NOT run_rc EQUAL 0)
  message(FATAL_ERROR "CLI run failed: ${run_rc}\nSTDOUT:\n${run_out}\nSTDERR:\n${run_err}")
endif()

if(NOT EXISTS "${OUT_FILE}")
  message(FATAL_ERROR "crypto_opportunities.csv was not produced")
endif()

file(READ "${OUT_FILE}" out_csv)
string(REGEX MATCHALL "[^\r\n]+" csv_lines "${out_csv}")
list(LENGTH csv_lines csv_line_count)
if(csv_line_count LESS 2)
  message(FATAL_ERROR "Expected opportunities output to contain data rows, got:\n${out_csv}")
endif()

string(REGEX MATCH "observed_at,symbol,buy_exchange,sell_exchange,buy_ask,sell_bid,gross_spread,net_spread,net_pct" has_header "${out_csv}")
if(has_header STREQUAL "")
  message(FATAL_ERROR "Opportunities output header is invalid:\n${out_csv}")
endif()

# CMake regex is more portable here without {n} quantifiers across versions.
string(REGEX MATCH
  "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z,XTZUSD,"
  has_timestamp_and_symbol
  "${out_csv}")
if(has_timestamp_and_symbol STREQUAL "")
  message(FATAL_ERROR "Expected at least one XTZUSD row with non-empty UTC observed_at, got:\n${out_csv}")
endif()
