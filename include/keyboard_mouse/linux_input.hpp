#pragma once

#include "keyboard_mouse/config.hpp"
#include "keyboard_mouse/controller.hpp"

#include <filesystem>
#include <string>
#include <vector>

namespace km {

struct InputDeviceInfo {
    std::filesystem::path path;
    std::string name;
    bool accessible = false;
};

std::vector<InputDeviceInfo> discover_keyboards(const std::vector<std::string>& names);
int run_controller(const Config& config, bool exclusive);
bool stop_running_controller();
Config calibrate(const std::vector<std::string>& names);

} // namespace km
