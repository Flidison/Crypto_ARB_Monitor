#include "config/ConfigManager.h"
#include "exceptions/Exceptions.h"

#include <filesystem>
#include <iostream>
#include <string>

namespace fs = std::filesystem;

static void print_usage() {
    std::cerr
        << "Usage: arbitrage_monitor <command> [options]\n"
        << "Commands:\n"
        << "  run    [--config <path>]  Run once and write report\n"
        << "  watch  [--config <path>]  Run continuously\n"
        << "  config [--config <path>]  Print effective configuration\n";
}

static std::string resolve_from_config(const std::string& cfg_path,
                                       const std::string& p) {
    if (fs::path(p).is_absolute()) return p;
    return (fs::path(cfg_path).parent_path() / p).string();
}

static void print_effective_config(const am::IConfigManager& cfg,
                                   const std::string& cfg_path) {
    auto s = [&](const std::string& k){
        auto v = cfg.get_string(k); return v ? *v : std::string("<unset>");
    };
    std::cout
        << "crypto_quotes_csv="
        << resolve_from_config(cfg_path, s("crypto_quotes_csv")) << "\n"
        << "crypto_output_csv="
        << resolve_from_config(cfg_path, s("crypto_output_csv")) << "\n"
        << "online_crypto_enabled=" << s("online_crypto_enabled") << "\n"
        << "watch_interval_sec="    << s("watch_interval_sec")    << "\n";
}

int main(int argc, char* argv[]) {
    const std::string cmd = (argc >= 2) ? argv[1] : "run";

    if (cmd != "run" && cmd != "watch" && cmd != "config") {
        print_usage(); return 1;
    }

    std::string config_path = "config/app.conf";
    for (int i = 2; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--config") config_path = argv[i + 1];

    try {
        am::ConfigManager cfg(config_path);
        cfg.load();
        if (cmd == "config") {
            print_effective_config(cfg, config_path);
            return 0;
        }
        std::cout << "arbitrage_monitor " << cmd << "\n";
    } catch (const am::AmException& e) {
        std::cerr << "Error: " << e.what() << "\n"; return 1;
    }
    return 0;
}
