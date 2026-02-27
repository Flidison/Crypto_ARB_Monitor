#include "config/ConfigManager.h"
#include "exceptions/Exceptions.h"
#include "common/StringUtils.h"

#include <fstream>

namespace am {

ConfigManager::ConfigManager(std::string path) : path_(std::move(path)) {}

void ConfigManager::load() {
    std::ifstream in(path_);
    if (!in) throw ConfigError("Cannot open config: " + path_);
    std::string line;
    while (std::getline(in, line)) {
        line = str::trim_copy(line);
        if (line.empty() || line[0] == '#') continue;
        const auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        auto key = str::trim_copy(line.substr(0, eq));
        auto val = str::trim_copy(line.substr(eq + 1));
        if (!key.empty()) kv_[key] = val;
    }
}

std::optional<std::string> ConfigManager::get_string(const std::string& key) const {
    auto it = kv_.find(key);
    if (it == kv_.end()) return std::nullopt;
    return it->second;
}

std::optional<double> ConfigManager::get_double(const std::string& key) const {
    auto s = get_string(key);
    if (!s) return std::nullopt;
    try { return std::stod(*s); }
    catch (...) { throw ConfigError("Bad double for key: " + key); }
}

std::optional<bool> ConfigManager::get_bool(const std::string& key) const {
    auto s = get_string(key);
    if (!s) return std::nullopt;
    if (*s == "true"  || *s == "1") return true;
    if (*s == "false" || *s == "0") return false;
    throw ConfigError("Bad bool for key: " + key);
}

} // namespace am
