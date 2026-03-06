cmake_minimum_required(VERSION 3.20)
project(ArbitrageMonitor VERSION 1.0.0 LANGUAGES CXX)


if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(AM_BUILD_TESTS "Build tests" ON)

# ── Core library ──────────────────────────────────────────────────────────────
add_library(am_core
    src/ConfigManager.cpp
    src/CryptoArbitrageEngine.cpp
    src/MarketDataConnectors.cpp
)
target_include_directories(am_core PUBLIC include)
target_compile_features(am_core PUBLIC cxx_std_23)

# ── CLI executable ────────────────────────────────────────────────────────────
add_executable(arbitrage_monitor src/main.cpp)
target_link_libraries(arbitrage_monitor PRIVATE am_core)

# ── Tests ─────────────────────────────────────────────────────────────────────
if(AM_BUILD_TESTS)
    add_subdirectory(external/googletest)
    enable_testing()
    add_executable(am_tests
        tests/test_config_manager.cpp
    tests/test_crypto_arbitrage.cpp
    tests/test_online_connectors.cpp
    tests/test_tradingview_quotes.cpp
    )
    target_link_libraries(am_tests PRIVATE am_core gtest_main)
    target_include_directories(am_tests PRIVATE include)
    add_test(NAME am_tests COMMAND am_tests)
endif()

# ── CLI e2e tests ─────────────────────────────────────────────────────────────
if(AM_BUILD_TESTS)
    add_test(
        NAME cli_e2e
        COMMAND ${CMAKE_COMMAND}
            -DMONITOR_BIN=$<TARGET_FILE:arbitrage_monitor>
            -P "${CMAKE_SOURCE_DIR}/tests/run_cli_e2e.cmake"
    )
endif()
    add_test(
        NAME cli_config_and_args
        COMMAND ${CMAKE_COMMAND}
            -DMONITOR_BIN=$<TARGET_FILE:arbitrage_monitor>
            -P "${CMAKE_SOURCE_DIR}/tests/run_cli_config_and_args.cmake"
    )
