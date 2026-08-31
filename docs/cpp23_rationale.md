# C++23 Rationale

The project deliberately configures `CMAKE_CXX_STANDARD` as `23` and disables compiler-specific language extensions.

## Required C++23 Library Feature

`src/main.cpp` uses `std::string::contains` while classifying fixture and test paths. This member function was added in C++23 and is the direct reason the current source requires a C++23-capable standard library.

The same checks could be expressed with `find` if the project later needed an older language target, but changing the advertised standard is outside the current maintenance scope.

## Other Modern C++ Usage

The codebase also uses features introduced before C++23:

- `std::optional` represents missing configuration values, parser results, and nullable online fields without sentinel values.
- `std::variant` and `std::visit` model the mutually exclusive offline, TradingView, and direct-API quote sources.
- `std::unique_ptr` owns runtime interfaces with a single owner; `std::shared_ptr` keeps the arbitrage engine alive across the captured iteration callable.
- Structured bindings simplify iteration over normalized quote maps.
- `constexpr` names the basis-point denominator and TradingView source identifier.
- RAII manages file streams and the `PipeReader` process handle, including exception paths.

These are design choices rather than additional reasons to require the C++23 language level.

## Relevant Locations

- Runtime dispatch and `std::string::contains`: `src/main.cpp`
- Quote parsing and process RAII: `src/online/MarketDataConnectors.cpp`
- Fee calculation constant: `src/crypto/CryptoArbitrageEngine.cpp`
- Optional-bearing interfaces: `include/config/ConfigManager.h` and `include/online/MarketDataConnectors.h`
