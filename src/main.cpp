#include <iostream>
#include <string>

static void print_usage() {
    std::cerr
        << "Usage: arbitrage_monitor <command> [options]\n"
        << "Commands:\n"
        << "  run    [--config <path>]  Run once and write report\n"
        << "  watch  [--config <path>]  Run continuously\n"
        << "  config [--config <path>]  Print effective configuration\n";
}

int main(int argc, char* argv[]) {
    const std::string cmd = (argc >= 2) ? argv[1] : "run";

    if (cmd != "run" && cmd != "watch" && cmd != "config") {
        print_usage();
        return 1;
    }

    std::string config_path = "config/app.conf";
    for (int i = 2; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--config") config_path = argv[i + 1];

    std::cout << "arbitrage_monitor " << cmd
              << "  config=" << config_path << "\n";
    return 0;
}
