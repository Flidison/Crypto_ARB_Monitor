#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>

#include "config/ConfigManager.h"
#include "exceptions/Exceptions.h"

namespace {

std::string make_temp_config_path(const std::string& test_name) {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const auto path = std::filesystem::temp_directory_path() /
                      std::filesystem::path("am_cfg_" + test_name + "_" + std::to_string(stamp) + ".conf");
    return path.string();
}

std::string write_temp_config(const std::string& test_name, const std::string& body) {
    const std::string path = make_temp_config_path(test_name);
    std::ofstream out(path, std::ios::binary);
    out << body;
    out.close();
    return path;
}

} // namespace

TEST(ConfigManager, LoadsAndTrimsValues) {
    const std::string path = write_temp_config(
        "loads_and_trims",
        "# comment\n"
        " name =   monitor  \n"
        " fee_bps =  12.5 \n"
        " enabled = true\n");

    am::ConfigManager cfg(path);
    cfg.load();

    ASSERT_EQ(cfg.get_string("name").has_value(), true);
    EXPECT_EQ(*cfg.get_string("name"), "monitor");
    ASSERT_EQ(cfg.get_double("fee_bps").has_value(), true);
    EXPECT_NEAR(*cfg.get_double("fee_bps"), 12.5, 1e-12);
    ASSERT_EQ(cfg.get_bool("enabled").has_value(), true);
    EXPECT_EQ(*cfg.get_bool("enabled"), true);

    std::filesystem::remove(path);
}

TEST(ConfigManager, ThrowsOnBadDouble) {
    const std::string path = write_temp_config("bad_double", "fee_bps = nope\n");
    am::ConfigManager cfg(path);
    cfg.load();

    EXPECT_THROW((void)cfg.get_double("fee_bps"), am::ConfigError);
    std::filesystem::remove(path);
}

TEST(ConfigManager, ThrowsOnBadBool) {
    const std::string path = write_temp_config("bad_bool", "enabled = maybe\n");
    am::ConfigManager cfg(path);
    cfg.load();

    EXPECT_THROW((void)cfg.get_bool("enabled"), am::ConfigError);
    std::filesystem::remove(path);
}

TEST(ConfigManager, ThrowsWhenFileMissing) {
    const std::string path = make_temp_config_path("missing");
    am::ConfigManager cfg(path);
    EXPECT_THROW(cfg.load(), am::ConfigError);
}
