cmake_minimum_required(VERSION 3.20)

if(NOT DEFINED MONITOR_BIN)
    message(FATAL_ERROR "MONITOR_BIN not set")
endif()

set(TMP_DIR "${CMAKE_CURRENT_BINARY_DIR}/e2e_tmp")
file(MAKE_DIRECTORY "${TMP_DIR}")

set(QUOTES_FILE "${TMP_DIR}/quotes.csv")
file(WRITE "${QUOTES_FILE}"
"exchange,symbol,bid,ask,fee_bps\n"
"BINANCE,XTZUSD,1.1050,1.1060,8.0\n"
"KRAKEN,XTZUSD,1.1075,1.1085,12.0\n"
"BITSTAMP,XTZUSD,1.1068,1.1078,10.0\n"
)

set(OUT_FILE    "${TMP_DIR}/opportunities.csv")
set(CONFIG_FILE "${TMP_DIR}/app.conf")
file(WRITE "${CONFIG_FILE}"
"crypto_quotes_csv = quotes.csv\n"
"crypto_output_csv = opportunities.csv\n"
"online_crypto_enabled = false\n"
)

execute_process(
    COMMAND "${MONITOR_BIN}" run --config "${CONFIG_FILE}"
    RESULT_VARIABLE ret
    OUTPUT_VARIABLE out_stdout
    ERROR_VARIABLE  out_stderr
)
if(NOT ret EQUAL 0)
    message(FATAL_ERROR "CLI run failed: ${out_stderr}")
endif()

file(READ "${OUT_FILE}" out_csv)
string(FIND "${out_csv}" "observed_at,symbol,buy_exchange" has_header)
if(has_header EQUAL -1)
    message(FATAL_ERROR "Missing CSV header in output:\n${out_csv}")
endif()

# Portable regex без {n} квантификаторов (совместимость с CMake < 3.9)
string(REGEX MATCH
    "[0-9][0-9][0-9][0-9]-[0-9][0-9]-[0-9][0-9]T[0-9][0-9]:[0-9][0-9]:[0-9][0-9]Z,XTZUSD,"
    has_timestamp_and_symbol
    "${out_csv}"
)
if(NOT has_timestamp_and_symbol)
    message(FATAL_ERROR "No valid timestamp+symbol row in output:\n${out_csv}")
endif()

message(STATUS "CLI e2e test PASSED")
