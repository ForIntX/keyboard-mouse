#include "keyboard_mouse/config.hpp"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace km {
namespace {

std::vector<std::string> split(const std::string& line, char separator) {
    std::vector<std::string> result;
    std::stringstream stream(line);
    std::string part;
    while (std::getline(stream, part, separator)) {
        result.push_back(part);
    }
    return result;
}

} // namespace

std::filesystem::path default_config_path() {
    if (const char* value = std::getenv("XDG_CONFIG_HOME"); value && *value) {
        return std::filesystem::path(value) / "keyboard-mouse" / "config.conf";
    }
    if (const char* home = std::getenv("HOME"); home && *home) {
        return std::filesystem::path(home) / ".config" / "keyboard-mouse" / "config.conf";
    }
    throw std::runtime_error("HOME veya XDG_CONFIG_HOME ayarlanmamis");
}

Config load_config(const std::filesystem::path& path) {
    std::ifstream input(path);
    if (!input) {
        throw std::runtime_error("Yapilandirma okunamadi: " + path.string() +
                                 " (once --calibrate calistirin)");
    }

    Config config;
    std::string line;
    int line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (line.empty() || line[0] == '#') {
            continue;
        }
        const auto equal = line.find('=');
        if (equal == std::string::npos) {
            throw std::runtime_error("Gecersiz ayar satiri " + std::to_string(line_number));
        }
        const std::string key = line.substr(0, equal);
        const std::string value = line.substr(equal + 1);
        if (key == "start_speed") {
            config.start_speed = std::stod(value);
        } else if (key == "max_speed") {
            config.max_speed = std::stod(value);
        } else if (key == "acceleration_seconds") {
            config.acceleration_seconds = std::stod(value);
        } else if (key == "device") {
            const auto fields = split(value, '\t');
            if (fields.size() != 6) {
                throw std::runtime_error("Gecersiz device ayari, satir " +
                                         std::to_string(line_number));
            }
            DeviceConfig device;
            device.name = fields[0];
            device.trigger = fields[1] == "fn-ctrl" ? TriggerKind::FnCtrl
                                                     : TriggerKind::CapsLock;
            device.fn_code = std::stoi(fields[2]);
            device.left_click_code = std::stoi(fields[3]);
            device.right_click_code = std::stoi(fields[4]);
            device.middle_click_code = std::stoi(fields[5]);
            config.devices.push_back(device);
        }
    }

    if (config.devices.empty()) {
        throw std::runtime_error("Yapilandirmada klavye yok; --calibrate calistirin");
    }
    if (config.start_speed <= 0 || config.max_speed < config.start_speed ||
        config.acceleration_seconds <= 0) {
        throw std::runtime_error("Hiz ayarlari gecersiz");
    }
    return config;
}

void save_config(const Config& config, const std::filesystem::path& path) {
    std::filesystem::create_directories(path.parent_path());
    const auto temporary = path.string() + ".tmp";
    std::ofstream output(temporary, std::ios::trunc);
    if (!output) {
        throw std::runtime_error("Yapilandirma yazilamadi: " + temporary);
    }
    output << "# keyboard-mouse configuration v1\n";
    output << std::fixed << std::setprecision(2);
    output << "start_speed=" << config.start_speed << '\n';
    output << "max_speed=" << config.max_speed << '\n';
    output << "acceleration_seconds=" << config.acceleration_seconds << '\n';
    for (const auto& device : config.devices) {
        if (device.name.find('\t') != std::string::npos || device.name.find('\n') != std::string::npos) {
            throw std::runtime_error("Klavye adi ayar dosyasina yazilamiyor");
        }
        output << "device=" << device.name << '\t'
               << (device.trigger == TriggerKind::FnCtrl ? "fn-ctrl" : "caps") << '\t'
               << device.fn_code << '\t' << device.left_click_code << '\t'
               << device.right_click_code << '\t' << device.middle_click_code << '\n';
    }
    output.close();
    if (!output) {
        throw std::runtime_error("Yapilandirma tamamlanamadi: " + temporary);
    }
    std::filesystem::rename(temporary, path);
}

std::string trigger_name(TriggerKind trigger) {
    return trigger == TriggerKind::FnCtrl ? "Fn + Ctrl" : "Caps Lock (basili tut)";
}

} // namespace km
