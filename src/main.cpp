#include "keyboard_mouse/config.hpp"
#include "keyboard_mouse/linux_input.hpp"

#include <csignal>
#include <algorithm>
#include <cctype>
#include <filesystem>
#include <iostream>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

extern "C" void keyboard_mouse_signal_handler(int);

#ifndef KEYBOARD_MOUSE_VERSION
#define KEYBOARD_MOUSE_VERSION "development"
#endif

namespace {

// Bos filtre, Linux EV_KEY yeteneklerine sahip tum gercek klavyeleri secer.
const std::vector<std::string> kAllKeyboards;

void print_help(const char* executable) {
    std::cout << "Kullanim:\n"
              << "  " << executable << "                 Fare denetimini baslat\n"
              << "  " << executable << " --safe          Klavyeyi kilitlemeyen yedek mod\n"
              << "  " << executable << " --calibrate     Klavyeleri ve tuslari tanit\n"
              << "  " << executable << " --devices       Kullanilacak klavyeleri sec\n"
              << "  " << executable << " --status        Aygit ve esleme durumunu goster\n"
              << "  " << executable << " --stop          Calisan sureci guvenle durdur\n"
              << "  " << executable << " --version       Kurulu surumu goster\n"
              << "  " << executable << " --help          Bu yardimi goster\n";
}

bool ask_yes_no(const std::string& question, bool default_yes) {
    while (true) {
        std::cout << question << (default_yes ? " [E/h]: " : " [e/H]: ") << std::flush;
        std::string answer;
        if (!std::getline(std::cin, answer)) {
            throw std::runtime_error("Klavye secimi okunamadi");
        }
        if (answer.empty()) return default_yes;
        const char first = static_cast<char>(std::tolower(static_cast<unsigned char>(answer[0])));
        if (first == 'e' || first == 'y') return true;
        if (first == 'h' || first == 'n') return false;
        std::cout << "Lutfen e veya h girin.\n";
    }
}

void configure_devices(const std::filesystem::path& config_path, bool recalibrate) {
    km::Config existing;
    bool has_existing = false;
    try {
        existing = km::load_config(config_path);
        has_existing = true;
    } catch (const std::exception&) {
        // Ilk kurulumda ayar dosyasinin bulunmamasi normaldir.
    }

    const auto discovered = km::discover_keyboards(kAllKeyboards);
    std::set<std::string> connected;
    for (const auto& device : discovered) connected.insert(device.name);

    std::map<std::string, km::DeviceConfig> previous;
    for (const auto& device : existing.devices) previous[device.name] = device;

    std::vector<std::string> candidates(connected.begin(), connected.end());
    for (const auto& [name, device] : previous) {
        (void)device;
        if (connected.count(name) == 0) candidates.push_back(name);
    }
    if (candidates.empty()) {
        throw std::runtime_error(
            "Bagli veya daha once kaydedilmis klavye yok. Izinleri ve baglantiyi kontrol edin.");
    }

    std::cout << "\nKlavye baglanti secimi:\n";
    std::vector<std::string> selected;
    for (const auto& name : candidates) {
        const bool is_connected = connected.count(name) != 0;
        const bool was_selected = previous.count(name) != 0;
        std::cout << "- " << name << " ["
                  << (is_connected ? "bagli" : "bagli degil; profil kayitli") << "]\n";
        if (ask_yes_no("  Bu klavye kullanilsin mi?", was_selected || is_connected)) {
            selected.push_back(name);
        }
    }
    if (selected.empty()) {
        throw std::runtime_error("En az bir klavye secilmelidir; ayarlar degistirilmedi");
    }

    std::vector<std::string> needs_calibration;
    for (const auto& name : selected) {
        const bool is_connected = connected.count(name) != 0;
        const bool is_new = previous.count(name) == 0;
        if (is_connected && (recalibrate || is_new)) needs_calibration.push_back(name);
    }

    std::map<std::string, km::DeviceConfig> calibrated;
    if (!needs_calibration.empty()) {
        const auto calibration = km::calibrate(needs_calibration);
        for (const auto& device : calibration.devices) calibrated[device.name] = device;
    }

    km::Config result;
    if (has_existing) {
        result.start_speed = existing.start_speed;
        result.max_speed = existing.max_speed;
        result.acceleration_seconds = existing.acceleration_seconds;
    }
    for (const auto& name : selected) {
        if (const auto calibrated_device = calibrated.find(name);
            calibrated_device != calibrated.end()) {
            result.devices.push_back(calibrated_device->second);
        } else if (const auto previous_device = previous.find(name);
                   previous_device != previous.end()) {
            result.devices.push_back(previous_device->second);
        } else {
            km::DeviceConfig defaults;
            defaults.name = name;
            result.devices.push_back(defaults);
        }
    }
    km::save_config(result, config_path);
    std::cout << "\nKlavye secimi kaydedildi: " << config_path << '\n';
    for (const auto& device : result.devices) {
        std::cout << "  " << device.name << " ["
                  << (connected.count(device.name) != 0 ? "hazir" : "yeniden baglanmasi bekleniyor")
                  << "]\n";
    }
}

void print_status(const std::filesystem::path& config_path) {
    std::cout << "Ayar dosyasi: " << config_path << '\n';
    const auto devices = km::discover_keyboards(kAllKeyboards);
    std::set<std::string> connected_names;
    for (const auto& device : devices) connected_names.insert(device.name);
    km::Config config;
    try {
        config = km::load_config(config_path);
        for (const auto& item : config.devices) {
            std::cout << "Esleme: " << item.name << " | " << km::trigger_name(item.trigger)
                      << " | "
                      << (connected_names.count(item.name) != 0 ? "bagli" : "baglanti bekleniyor")
                      << " | + sol, - sag, 0 orta"
                      << " | kalibre kodlar=" << item.left_click_code << ','
                      << item.right_click_code << ',' << item.middle_click_code
                      << " | ana sira + numpad destekli" << '\n';
        }
    } catch (const std::exception& error) {
        std::cout << "Esleme: hazir degil (" << error.what() << ")\n";
    }

    if (devices.empty()) {
        std::cout << "Aygit: klavye gorunmuyor veya /dev/input erisilemiyor.\n";
    }
    for (const auto& device : devices) {
        std::cout << "Aygit: " << device.name << " | " << device.path << " | "
                  << (device.accessible ? "erisilebilir" : "izin gerekli") << '\n';
    }
    std::cout << "/dev/uinput: "
              << (access("/dev/uinput", R_OK | W_OK) == 0 ? "erisilebilir" : "izin gerekli")
              << '\n';
}

} // namespace

int main(int argc, char** argv) {
    try {
        const auto config_path = km::default_config_path();
        const std::string option = argc > 1 ? argv[1] : "";
        if (option == "--help" || option == "-h") {
            print_help(argv[0]);
            return 0;
        }
        if (option == "--status") {
            print_status(config_path);
            return 0;
        }
        if (option == "--version") {
            std::cout << "keyboard-mouse " << KEYBOARD_MOUSE_VERSION
                      << " (+ sol tik, - sag tik, 0 orta tik)\n";
            return 0;
        }
        if (option == "--stop") {
            if (km::stop_running_controller()) {
                std::cout << "Kapatma sinyali gonderildi.\n";
                return 0;
            }
            std::cout << "Calisan keyboard-mouse bulunamadi.\n";
            return 1;
        }
        if (option == "--calibrate") {
            configure_devices(config_path, true);
            return 0;
        }
        if (option == "--devices") {
            configure_devices(config_path, false);
            return 0;
        }
        const bool safe = option == "--safe";
        if (!option.empty() && !safe) {
            std::cerr << "Bilinmeyen secenek: " << option << "\n";
            print_help(argv[0]);
            return 2;
        }

        std::signal(SIGINT, keyboard_mouse_signal_handler);
        std::signal(SIGTERM, keyboard_mouse_signal_handler);
        const bool exclusive = !safe;
        std::cout << "keyboard-mouse "
                  << (exclusive ? "EXCLUSIVE modda" : "SAFE modda")
                  << " baslatiliyor. Durdurmak icin Ctrl+C veya Ctrl+Alt+Esc.\n";
        return km::run_controller(km::load_config(config_path), exclusive);
    } catch (const std::exception& error) {
        std::cerr << "Hata: " << error.what() << '\n';
        return 1;
    }
}
