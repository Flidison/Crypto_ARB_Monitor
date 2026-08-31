cmake_minimum_required(VERSION 3.20)
project(ArbitrageMonitor VERSION 1.0.0 LANGUAGES CXX)

# CMP0135 NEW makes FetchContent archive timestamps consistent and avoids stale rebuild behavior.
if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(AM_BUILD_TESTS "Build tests" ON)

add_library(am_core
    src/config/ConfigManager.cpp
    src/crypto/CryptoArbitrageEngine.cpp
    src/online/MarketDataConnectors.cpp
)

target_include_directories(am_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(am_core PUBLIC cxx_std_23)

if (MSVC)
    target_compile_options(am_core PRIVATE /W4)
else()
    target_compile_options(am_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(arbitrage_monitor src/main.cpp)
target_link_libraries(arbitrage_monitor PRIVATE am_core)

if (AM_BUILD_TESTS)
    add_subdirectory(external/googletest)
    enable_testing()

    add_executable(am_tests
        tests/test_config_manager.cpp
        tests/test_crypto_arbitrage.cpp
        tests/test_online_connectors.cpp
        tests/test_tradingview_quotes.cpp
    )
    target_link_libraries(am_tests PRIVATE am_core gtest_main)

    add_test(NAME am_tests COMMAND am_tests)

    add_test(
        NAME cli_e2e
        COMMAND ${CMAKE_COMMAND}
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
            -DMONITOR_BIN=$<TARGET_FILE:arbitrage_monitor>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_e2e.cmake
    )
    add_test(
        NAME cli_config_and_args
        COMMAND ${CMAKE_COMMAND}
            -DREPO_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -DWORKDIR=${CMAKE_CURRENT_BINARY_DIR}
            -DMONITOR_BIN=$<TARGET_FILE:arbitrage_monitor>
            -P ${CMAKE_CURRENT_SOURCE_DIR}/tests/run_cli_config_and_args.cmake
    )
endif()
