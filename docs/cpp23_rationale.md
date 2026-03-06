# C++23 Feature Rationale

## `std::optional`
Used for all nullable return values (config getters, JSON parsers).
Eliminates sentinel values and makes absence explicit.

## `std::variant` + `std::visit`
Models mutually exclusive quote-source modes (offline CSV vs online direct
vs TradingView) without inheritance overhead.

## `std::filesystem`
Cross-platform path resolution in `resolve_from_config`.

## Ranges and structured bindings
Used in engine for cleaner iteration over quote pairs and opportunity maps.

## `[[nodiscard]]`
Applied to all factory and fetch functions to prevent silent error ignoring.
