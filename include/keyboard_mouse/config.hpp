#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace km {

enum class TriggerKind { CapsLock, FnCtrl };

struct DeviceConfig {
    std::string name;
    TriggerKind trigger = TriggerKind::CapsLock;
    int fn_code = -1;
    int left_click_code = 13;   // KEY_EQUAL / '+' on common layouts
    int right_click_code = 12;  // KEY_MINUS
    int middle_click_code = 11; // KEY_0
};

struct Config {
    std::vector<DeviceConfig> devices;
    double start_speed = 180.0;
    double max_speed = 1080.0;
    double acceleration_seconds = 1.2;
};

std::filesystem::path default_config_path();
Config load_config(const std::filesystem::path& path);
void save_config(const Config& config, const std::filesystem::path& path);
std::string trigger_name(TriggerKind trigger);

} // namespace km
