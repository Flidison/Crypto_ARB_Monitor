#pragma once
#include <stdexcept>
#include <string>

namespace am {

class AmException : public std::runtime_error {
public:
    explicit AmException(const std::string& msg) : std::runtime_error(msg) {}
};

class ConfigError : public AmException {
public:
    explicit ConfigError(const std::string& msg) : AmException(msg) {}
};

class CsvError : public AmException {
public:
    explicit CsvError(const std::string& msg) : AmException(msg) {}
};

class DataValidationError : public AmException {
public:
    explicit DataValidationError(const std::string& msg) : AmException(msg) {}
};

class CalculationError : public AmException {
public:
    explicit CalculationError(const std::string& msg) : AmException(msg) {}
};

} // namespace am
