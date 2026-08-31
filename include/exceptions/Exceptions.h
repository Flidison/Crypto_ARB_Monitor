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
    explicit ConfigError(const std::string& msg) : AmException("ConfigError: " + msg) {}
};

class CsvError : public AmException {
public:
    explicit CsvError(const std::string& msg) : AmException("CsvError: " + msg) {}
};

class DataValidationError : public AmException {
public:
    explicit DataValidationError(const std::string& msg) : AmException("DataValidationError: " + msg) {}
};

class CalculationError : public AmException {
public:
    explicit CalculationError(const std::string& msg) : AmException("CalculationError: " + msg) {}
};

} // namespace am
