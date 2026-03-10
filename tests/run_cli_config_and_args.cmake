if(NOT DEFINED REPO_ROOT OR NOT DEFINED WORKDIR OR NOT DEFINED MONITOR_BIN)
  message(FATAL_ERROR "REPO_ROOT, WORKDIR and MONITOR_BIN must be defined")
endif()

function(assert_output_contains_path output prefix expected_path)
  # Path checks accept native and slash-normalized forms for cross-platform stability.
  string(REPLACE "\\" "/" expected_unix "${expected_path}")
  string(REPLACE "/" "\\" expected_win "${expected_unix}")

  string(FIND "${output}" "${prefix}${expected_path}" found_native)
  string(FIND "${output}" "${prefix}${expected_unix}" found_unix)
  string(FIND "${output}" "${prefix}${expected_win}" found_win)
  if(found_native EQUAL -1 AND found_unix EQUAL -1 AND found_win EQUAL -1)
    message(FATAL_ERROR "Expected output to contain '${prefix}${expected_path}', but got:\n${output}")
  endif()
endfunction()

set(TEST_DIR "${WORKDIR}/cli_config_validation")
file(REMOVE_RECURSE "${TEST_DIR}")
file(MAKE_DIRECTORY "${TEST_DIR}/nested")

set(CONFIG_FILE "${TEST_DIR}/nested/app.conf")
file(WRITE "${CONFIG_FILE}"
  "crypto_quotes_csv = data/quotes.csv\n"
  "crypto_fees_csv = data/fees.csv\n"
  "crypto_output_csv = out/opps.csv\n"
  "profit_output_csv = out/profit.csv\n"
  "crypto_symbol = XTZUSD\n"
  "online_crypto_enabled = false\n"
  "watch_interval_sec = 5\n"
  "profit_calc_enabled = false\n"
)

execute_process(
  COMMAND "${MONITOR_BIN}" config --config "${CONFIG_FILE}"
  WORKING_DIRECTORY "${WORKDIR}"
  RESULT_VARIABLE cfg_rc
  OUTPUT_VARIABLE cfg_out
  ERROR_VARIABLE cfg_err
)
if(NOT cfg_rc EQUAL 0)
  message(FATAL_ERROR "CLI config command failed: ${cfg_rc}\nSTDOUT:\n${cfg_out}\nSTDERR:\n${cfg_err}")
endif()

cmake_path(GET CONFIG_FILE PARENT_PATH CONFIG_DIR)
set(EXPECTED_QUOTES "${CONFIG_DIR}/data/quotes.csv")
set(EXPECTED_FEES "${CONFIG_DIR}/data/fees.csv")
set(EXPECTED_OUT "${CONFIG_DIR}/out/opps.csv")
set(EXPECTED_PROFIT "${CONFIG_DIR}/out/profit.csv")
cmake_path(NORMAL_PATH EXPECTED_QUOTES)
cmake_path(NORMAL_PATH EXPECTED_FEES)
cmake_path(NORMAL_PATH EXPECTED_OUT)
cmake_path(NORMAL_PATH EXPECTED_PROFIT)

assert_output_contains_path("${cfg_out}" "crypto_quotes_csv=" "${EXPECTED_QUOTES}")
assert_output_contains_path("${cfg_out}" "crypto_fees_csv=" "${EXPECTED_FEES}")
assert_output_contains_path("${cfg_out}" "crypto_output_csv=" "${EXPECTED_OUT}")
assert_output_contains_path("${cfg_out}" "profit_output_csv=" "${EXPECTED_PROFIT}")

execute_process(
  COMMAND "${MONITOR_BIN}" run --config "${CONFIG_FILE}" --unknown-opt 1
  WORKING_DIRECTORY "${WORKDIR}"
  RESULT_VARIABLE bad_rc
  OUTPUT_VARIABLE bad_out
  ERROR_VARIABLE bad_err
)
if(bad_rc EQUAL 0)
  message(FATAL_ERROR "Expected non-zero exit for unknown option, got 0.\nSTDOUT:\n${bad_out}\nSTDERR:\n${bad_err}")
endif()

string(FIND "${bad_err}" "Unknown CLI option: --unknown-opt" bad_opt_pos)
if(bad_opt_pos EQUAL -1)
  message(FATAL_ERROR "Missing unknown-option error in stderr.\nSTDOUT:\n${bad_out}\nSTDERR:\n${bad_err}")
endif()
