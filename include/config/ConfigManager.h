#pragma once
#include <optional>
#include <string>
#include <unordered_map>

namespace am {

class IConfigManager {
public:
    virtual ~IConfigManager() = default;
    virtual void load() = 0;
    virtual std::optional<std::string> get_string(const std::string& key) const = 0;
    virtual std::optional<double>      get_double(const std::string& key) const = 0;
    virtual std::optional<bool>        get_bool  (const std::string& key) const = 0;
};

class ConfigManager : public IConfigManager {
public:
    explicit ConfigManager(std::string path);
    void load() override;
    std::optional<std::string> get_string(const std::string& key) const override;
    std::optional<double>      get_double(const std::string& key) const override;
    std::optional<bool>        get_bool  (const std::string& key) const override;
private:
    std::string path_;
    std::unordered_map<std::string, std::string> kv_;
};

} // namespace am
