#include "keyboard_mouse/linux_input.hpp"

#include <linux/input.h>
#include <linux/uinput.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <iostream>
#include <limits.h>
#include <map>
#include <poll.h>
#include <set>
#include <stdexcept>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <thread>
#include <unistd.h>

namespace km {
namespace {

constexpr const char* kVirtualPrefix = "Keyboard Mouse Controller Virtual";
volatile std::sig_atomic_t g_running = 1;

std::string lock_path() {
    return "/tmp/keyboard-mouse-" + std::to_string(getuid()) + ".lock";
}

class InstanceLock {
public:
    InstanceLock() {
        const std::string path = lock_path();
        fd_ = open(path.c_str(), O_RDWR | O_CREAT | O_CLOEXEC | O_NOFOLLOW, 0600);
        if (fd_ < 0) {
            throw std::runtime_error("Tek kopya kilidi acilamadi: " +
                                     std::string(std::strerror(errno)));
        }
        struct stat info{};
        if (fstat(fd_, &info) < 0 || info.st_uid != getuid()) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("Tek kopya kilidi guvenli degil");
        }
        if (flock(fd_, LOCK_EX | LOCK_NB) < 0) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("keyboard-mouse zaten calisiyor");
        }
        if (ftruncate(fd_, 0) < 0) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("PID dosyasi temizlenemedi");
        }
        const std::string pid = std::to_string(getpid()) + "\n";
        if (write(fd_, pid.data(), pid.size()) != static_cast<ssize_t>(pid.size())) {
            close(fd_);
            fd_ = -1;
            throw std::runtime_error("PID dosyasi yazilamadi");
        }
    }
    ~InstanceLock() {
        if (fd_ >= 0) {
            const int ignored = ftruncate(fd_, 0);
            (void)ignored;
            close(fd_);
        }
    }
    InstanceLock(const InstanceLock&) = delete;
    InstanceLock& operator=(const InstanceLock&) = delete;

private:
    int fd_ = -1;
};

bool bit_is_set(const unsigned long* bits, int bit) {
    constexpr int width = static_cast<int>(sizeof(unsigned long) * 8);
    return (bits[bit / width] & (1UL << (bit % width))) != 0;
}

bool is_button_code(int code) {
    return (code >= BTN_MISC && code <= BTN_GEAR_UP) ||
           (code >= BTN_DPAD_UP && code <= BTN_DPAD_RIGHT) ||
           (code >= BTN_TRIGGER_HAPPY1 && code <= BTN_TRIGGER_HAPPY40);
}

template <std::size_t N>
bool get_bits(int fd, int event_type, std::array<unsigned long, N>& bits) {
    bits.fill(0);
    return ioctl(fd, EVIOCGBIT(event_type, sizeof(bits)), bits.data()) >= 0;
}

bool is_keyboard(int fd) {
    std::array<unsigned long, (KEY_MAX / (sizeof(unsigned long) * 8)) + 1> keys{};
    return get_bits(fd, EV_KEY, keys) && bit_is_set(keys.data(), KEY_A) &&
           bit_is_set(keys.data(), KEY_LEFT) && bit_is_set(keys.data(), KEY_ENTER);
}

std::string device_name(int fd) {
    std::array<char, 256> name{};
    if (ioctl(fd, EVIOCGNAME(name.size()), name.data()) < 0) {
        return {};
    }
    return name.data();
}

bool name_wanted(const std::string& name, const std::vector<std::string>& names) {
    return names.empty() || std::find(names.begin(), names.end(), name) != names.end();
}

std::vector<std::filesystem::path> event_paths() {
    std::vector<std::filesystem::path> result;
    DIR* directory = opendir("/dev/input");
    if (!directory) {
        return result;
    }
    while (dirent* entry = readdir(directory)) {
        const std::string name = entry->d_name;
        if (name.rfind("event", 0) == 0) {
            result.emplace_back(std::filesystem::path("/dev/input") / name);
        }
    }
    closedir(directory);
    std::sort(result.begin(), result.end());
    return result;
}

void write_event(int fd, const OutputEvent& event) {
    input_event raw{};
    raw.type = event.type;
    raw.code = event.code;
    raw.value = event.value;
    const char* data = reinterpret_cast<const char*>(&raw);
    std::size_t remaining = sizeof(raw);
    while (remaining > 0) {
        const ssize_t count = write(fd, data, remaining);
        if (count > 0) {
            data += count;
            remaining -= static_cast<std::size_t>(count);
        } else if (count < 0 && errno == EINTR) {
            continue;
        } else {
            throw std::runtime_error("Sanal aygita yazilamadi: " +
                                     std::string(std::strerror(errno)));
        }
    }
}

enum class VirtualKind { Keyboard, Mouse };

class VirtualDevice {
public:
    explicit VirtualDevice(VirtualKind kind) : kind_(kind) {
        fd_ = open("/dev/uinput", O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0) {
            throw std::runtime_error("/dev/uinput acilamadi: " +
                                     std::string(std::strerror(errno)) +
                                     ". Once ./install.sh calistirin.");
        }
        try {
            enable(EV_KEY);
            if (kind_ == VirtualKind::Keyboard) {
                enable(EV_LED);
                for (int code = 0; code <= KEY_MAX; ++code) {
                    if (is_button_code(code)) continue;
                    if (ioctl(fd_, UI_SET_KEYBIT, code) < 0) {
                        throw std::runtime_error("UI_SET_KEYBIT basarisiz");
                    }
                }
                for (int code = 0; code <= LED_MAX; ++code) {
                    enable_code(UI_SET_LEDBIT, code);
                }
            } else {
                enable_code(UI_SET_KEYBIT, BTN_LEFT);
                enable_code(UI_SET_KEYBIT, BTN_RIGHT);
                enable_code(UI_SET_KEYBIT, BTN_MIDDLE);
                enable(EV_REL);
                enable_code(UI_SET_RELBIT, REL_X);
                enable_code(UI_SET_RELBIT, REL_Y);
            }

            uinput_setup setup{};
            const char* name = kind_ == VirtualKind::Keyboard
                                   ? "Keyboard Mouse Controller Virtual Keyboard"
                                   : "Keyboard Mouse Controller Virtual Mouse";
            std::strncpy(setup.name, name, UINPUT_MAX_NAME_SIZE - 1);
            setup.id.bustype = BUS_VIRTUAL;
            setup.id.vendor = 0x1d6b;
            setup.id.product = kind_ == VirtualKind::Keyboard ? 0x4b4b : 0x4b4d;
            setup.id.version = 1;
            if (ioctl(fd_, UI_DEV_SETUP, &setup) < 0 || ioctl(fd_, UI_DEV_CREATE) < 0) {
                throw std::runtime_error("Sanal aygit olusturulamadi: " +
                                         std::string(std::strerror(errno)));
            }
            created_ = true;
            std::this_thread::sleep_for(std::chrono::milliseconds(150));
        } catch (...) {
            close(fd_);
            fd_ = -1;
            throw;
        }
    }

    ~VirtualDevice() {
        if (fd_ >= 0) {
            if (created_) {
                ioctl(fd_, UI_DEV_DESTROY);
            }
            close(fd_);
        }
    }

    VirtualDevice(const VirtualDevice&) = delete;
    VirtualDevice& operator=(const VirtualDevice&) = delete;

    int fd() const { return fd_; }
    void emit(const OutputEvent& event) { write_event(fd_, event); }

private:
    void enable(int event_type) {
        if (ioctl(fd_, UI_SET_EVBIT, event_type) < 0) {
            throw std::runtime_error("uinput olay turu etkinlestirilemedi");
        }
    }
    void enable_code(unsigned long request, int code) {
        if (ioctl(fd_, request, code) < 0) {
            throw std::runtime_error("uinput olay kodu etkinlestirilemedi");
        }
    }

    int fd_ = -1;
    bool created_ = false;
    VirtualKind kind_;
};

class PhysicalDevice {
public:
    PhysicalDevice(std::filesystem::path path, std::string name, int id,
                   const DeviceConfig& config, bool exclusive)
        : path_(std::move(path)), name_(std::move(name)), id_(id), config_(config) {
        exclusive_ = exclusive;
        const int mode = exclusive_ ? O_RDWR : O_RDONLY;
        fd_ = open(path_.c_str(), mode | O_NONBLOCK | O_CLOEXEC);
        if (fd_ < 0) {
            throw std::runtime_error(path_.string() + " acilamadi: " + std::strerror(errno));
        }
        if (exclusive_ && ioctl(fd_, EVIOCGRAB, 1) < 0) {
            const std::string message = path_.string() + " kilitlenemedi: " +
                                        std::strerror(errno);
            close(fd_);
            fd_ = -1;
            throw std::runtime_error(message);
        }
        grabbed_ = exclusive_;
    }

    ~PhysicalDevice() {
        if (fd_ >= 0) {
            if (grabbed_) {
                ioctl(fd_, EVIOCGRAB, 0);
            }
            close(fd_);
        }
    }

    PhysicalDevice(const PhysicalDevice&) = delete;
    PhysicalDevice& operator=(const PhysicalDevice&) = delete;
    PhysicalDevice(PhysicalDevice&& other) noexcept
        : path_(std::move(other.path_)), name_(std::move(other.name_)), id_(other.id_),
          config_(other.config_), fd_(other.fd_), grabbed_(other.grabbed_) {
        exclusive_ = other.exclusive_;
        left_ctrl_down_ = other.left_ctrl_down_;
        right_ctrl_down_ = other.right_ctrl_down_;
        left_alt_down_ = other.left_alt_down_;
        right_alt_down_ = other.right_alt_down_;
        other.fd_ = -1;
        other.grabbed_ = false;
        other.exclusive_ = false;
        other.left_ctrl_down_ = false;
        other.right_ctrl_down_ = false;
        other.left_alt_down_ = false;
        other.right_alt_down_ = false;
    }
    PhysicalDevice& operator=(PhysicalDevice&& other) noexcept {
        if (this == &other) {
            return *this;
        }
        if (fd_ >= 0) {
            if (grabbed_) ioctl(fd_, EVIOCGRAB, 0);
            close(fd_);
        }
        path_ = std::move(other.path_);
        name_ = std::move(other.name_);
        id_ = other.id_;
        config_ = other.config_;
        fd_ = other.fd_;
        grabbed_ = other.grabbed_;
        exclusive_ = other.exclusive_;
        left_ctrl_down_ = other.left_ctrl_down_;
        right_ctrl_down_ = other.right_ctrl_down_;
        left_alt_down_ = other.left_alt_down_;
        right_alt_down_ = other.right_alt_down_;
        other.fd_ = -1;
        other.grabbed_ = false;
        other.exclusive_ = false;
        other.left_ctrl_down_ = false;
        other.right_ctrl_down_ = false;
        other.left_alt_down_ = false;
        other.right_alt_down_ = false;
        return *this;
    }

    int fd() const { return fd_; }
    int id() const { return id_; }
    const std::filesystem::path& path() const { return path_; }
    const std::string& name() const { return name_; }
    const DeviceConfig& config() const { return config_; }

    bool requests_stop(const input_event& event) {
        if (event.type != EV_KEY) return false;
        const bool down = event.value != 0;
        if (event.code == KEY_LEFTCTRL) left_ctrl_down_ = down;
        if (event.code == KEY_RIGHTCTRL) right_ctrl_down_ = down;
        if (event.code == KEY_LEFTALT) left_alt_down_ = down;
        if (event.code == KEY_RIGHTALT) right_alt_down_ = down;
        const bool ctrl = left_ctrl_down_ || right_ctrl_down_;
        const bool alt = left_alt_down_ || right_alt_down_;
        // Ctrl+C normal kopyalama kisayoluyla cakisabilecegi icin yalnizca
        // ayri acil durum kombinasyonu burada dogrudan yakalanir. Terminaldeki
        // Ctrl+C normal SIGINT yolu uzerinden calismaya devam eder.
        return event.value == 1 && event.code == KEY_ESC && ctrl && alt;
    }

    void set_led(const input_event& event) const {
        if (exclusive_ && (event.type == EV_LED || event.type == EV_SYN)) {
            const ssize_t ignored = write(fd_, &event, sizeof(event));
            (void)ignored;
        }
    }

private:
    std::filesystem::path path_;
    std::string name_;
    int id_;
    DeviceConfig config_;
    int fd_ = -1;
    bool grabbed_ = false;
    bool exclusive_ = false;
    bool left_ctrl_down_ = false;
    bool right_ctrl_down_ = false;
    bool left_alt_down_ = false;
    bool right_alt_down_ = false;
};

bool ignored_calibration_key(int code) {
    return code == KEY_ENTER || code == KEY_ESC || code == KEY_LEFTCTRL ||
           code == KEY_RIGHTCTRL || code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT ||
           code == KEY_LEFTALT || code == KEY_RIGHTALT || code == KEY_CAPSLOCK;
}

void drain_events(int fd) {
    input_event event{};
    while (read(fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
    }
}

int capture_key(int fd, const std::string& prompt, int timeout_ms, bool fn_probe) {
    drain_events(fd);
    std::cout << prompt << std::flush;
    const auto deadline = std::chrono::steady_clock::now() +
                          std::chrono::milliseconds(timeout_ms);
    while (std::chrono::steady_clock::now() < deadline) {
        const auto remaining = std::chrono::duration_cast<std::chrono::milliseconds>(
            deadline - std::chrono::steady_clock::now());
        pollfd descriptor{fd, POLLIN, 0};
        const int result = poll(&descriptor, 1, static_cast<int>(remaining.count()));
        if (result <= 0) {
            break;
        }
        input_event event{};
        while (read(fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
            if (event.type == EV_KEY && event.value == 1 &&
                !ignored_calibration_key(event.code)) {
                std::cout << " algilandi (kod " << event.code << ")\n";
                return event.code;
            }
        }
    }
    if (fn_probe) {
        std::cout << " olay algilanmadi; Caps Lock yedegi secildi.\n";
    } else {
        std::cout << " zaman asimi.\n";
    }
    return -1;
}

const DeviceConfig* find_config(const Config& config, const std::string& name) {
    const auto found = std::find_if(config.devices.begin(), config.devices.end(),
                                    [&](const DeviceConfig& item) { return item.name == name; });
    return found == config.devices.end() ? nullptr : &*found;
}

void forward_led_events(int uinput_fd, const std::vector<PhysicalDevice>& devices) {
    input_event event{};
    while (read(uinput_fd, &event, sizeof(event)) == static_cast<ssize_t>(sizeof(event))) {
        if (event.type == EV_LED || event.type == EV_SYN) {
            for (const auto& device : devices) {
                device.set_led(event);
            }
        }
    }
}

bool stop_legacy_processes() {
    DIR* directory = opendir("/proc");
    if (!directory) return false;
    bool signalled = false;
    while (dirent* entry = readdir(directory)) {
        char* end = nullptr;
        const long parsed = std::strtol(entry->d_name, &end, 10);
        if (parsed <= 1 || *end != '\0' || parsed == getpid()) continue;
        const pid_t pid = static_cast<pid_t>(parsed);
        const std::string proc_path = "/proc/" + std::to_string(pid);
        struct stat info{};
        if (stat(proc_path.c_str(), &info) < 0 || info.st_uid != getuid()) continue;
        std::array<char, PATH_MAX + 1> executable{};
        const std::string exe_link = proc_path + "/exe";
        const ssize_t size = readlink(exe_link.c_str(), executable.data(), PATH_MAX);
        if (size <= 0) continue;
        executable[static_cast<std::size_t>(size)] = '\0';
        const std::string name = std::filesystem::path(executable.data()).filename().string();
        if (name.rfind("keyboard-mouse", 0) == 0 && kill(pid, SIGTERM) == 0) {
            signalled = true;
        }
    }
    closedir(directory);
    return signalled;
}

} // namespace

std::vector<InputDeviceInfo> discover_keyboards(const std::vector<std::string>& names) {
    std::vector<InputDeviceInfo> result;
    for (const auto& path : event_paths()) {
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            continue;
        }
        const std::string name = device_name(fd);
        const bool virtual_device = name.rfind(kVirtualPrefix, 0) == 0;
        const bool keyboard = !virtual_device && name_wanted(name, names) && is_keyboard(fd);
        close(fd);
        if (keyboard) {
            result.push_back({path, name, access(path.c_str(), R_OK | W_OK) == 0});
        }
    }
    return result;
}

bool stop_running_controller() {
    const std::string path = lock_path();
    const int fd = open(path.c_str(), O_RDWR | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) {
        if (errno == ENOENT) return stop_legacy_processes();
        throw std::runtime_error("PID dosyasi acilamadi: " + std::string(std::strerror(errno)));
    }
    struct stat lock_info{};
    if (fstat(fd, &lock_info) < 0 || lock_info.st_uid != getuid()) {
        close(fd);
        throw std::runtime_error("PID dosyasi bu kullaniciya ait degil");
    }

    // Kilidi alabiliyorsak aktif surec yoktur. Dosyadaki eski PID'yi kullanma.
    if (flock(fd, LOCK_EX | LOCK_NB) == 0) {
        flock(fd, LOCK_UN);
        close(fd);
        return stop_legacy_processes();
    }
    if (errno != EWOULDBLOCK && errno != EAGAIN) {
        close(fd);
        throw std::runtime_error("PID kilidi okunamadi");
    }

    std::array<char, 64> buffer{};
    const ssize_t count = read(fd, buffer.data(), buffer.size() - 1);
    close(fd);
    if (count <= 0) {
        return stop_legacy_processes();
    }
    char* end = nullptr;
    const long parsed = std::strtol(buffer.data(), &end, 10);
    if (parsed <= 1 || end == buffer.data()) {
        throw std::runtime_error("PID dosyasi gecersiz");
    }
    const pid_t pid = static_cast<pid_t>(parsed);

    const std::string proc_path = "/proc/" + std::to_string(pid);
    struct stat process_info{};
    if (stat(proc_path.c_str(), &process_info) < 0 || process_info.st_uid != getuid()) {
        throw std::runtime_error("PID bu kullaniciya ait bir surec degil");
    }
    std::array<char, PATH_MAX + 1> executable{};
    const std::string exe_link = proc_path + "/exe";
    const ssize_t exe_size = readlink(exe_link.c_str(), executable.data(), PATH_MAX);
    if (exe_size <= 0) {
        throw std::runtime_error("Calisan program dogrulanamadi");
    }
    executable[static_cast<std::size_t>(exe_size)] = '\0';
    const std::string executable_name =
        std::filesystem::path(executable.data()).filename().string();
    if (executable_name.rfind("keyboard-mouse", 0) != 0) {
        throw std::runtime_error("PID keyboard-mouse programina ait degil");
    }
    if (kill(pid, SIGTERM) < 0) {
        throw std::runtime_error("Program durdurulamadi: " + std::string(std::strerror(errno)));
    }
    return true;
}

Config calibrate(const std::vector<std::string>& names) {
    Config result;
    const auto discovered = discover_keyboards(names);
    if (discovered.empty()) {
        throw std::runtime_error(
            "Erisilebilir bir klavye bulunamadi. ./install.sh calistirip oturumu "
            "yeniden acmayi deneyin; mevcut ayar dosyasi degistirilmedi.");
    }
    std::map<std::string, InputDeviceInfo> by_name;
    for (const auto& device : discovered) {
        by_name.emplace(device.name, device);
    }

    std::vector<std::string> calibration_names = names;
    if (calibration_names.empty()) {
        for (const auto& [name, device] : by_name) {
            (void)device;
            calibration_names.push_back(name);
        }
    }

    std::cout << "Kalibrasyon basliyor. Fn testi sirasinda Fn tusuna birkac kez basin.\n"
                 "Diger adimlarda istenen fiziksel tusa bir kez basin.\n\n";
    for (const auto& name : calibration_names) {
        DeviceConfig config;
        config.name = name;
        const auto found = by_name.find(name);
        if (found == by_name.end()) {
            std::cout << "[UYARI] " << name
                      << " bagli/erisilebilir degil; varsayilan Caps Lock eslemesi kaydedildi.\n";
            result.devices.push_back(config);
            continue;
        }

        const int fd = open(found->second.path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            throw std::runtime_error(found->second.path.string() + " okunamadi: " +
                                     std::strerror(errno));
        }
        std::cout << "\nKlavye: " << name << " (" << found->second.path << ")\n";
        const int fn = capture_key(fd, "Fn tusuna basin (5 saniye): ", 5000, true);
        if (fn >= 0) {
            config.trigger = TriggerKind::FnCtrl;
            config.fn_code = fn;
        }
        const int left = capture_key(fd, "Sol tik icin kullanilacak '+' tusuna basin: ",
                                     15000, false);
        const int right = capture_key(fd, "Sag tik icin kullanilacak '-' tusuna basin: ",
                                      15000, false);
        const int middle = capture_key(fd, "Orta tik icin kullanilacak '0' tusuna basin: ",
                                       15000, false);
        close(fd);
        if (left >= 0) config.left_click_code = left;
        if (right >= 0) config.right_click_code = right;
        if (middle >= 0) config.middle_click_code = middle;
        result.devices.push_back(config);
    }
    return result;
}

int run_controller(const Config& config, bool exclusive) {
    g_running = 1;
    InstanceLock instance_lock;
    VirtualDevice virtual_keyboard(VirtualKind::Keyboard);
    VirtualDevice virtual_mouse(VirtualKind::Mouse);
    Controller controller(
        config,
        [&](const OutputEvent& event) {
            const bool mouse_button = event.type == EV_KEY &&
                                      (event.code == BTN_LEFT || event.code == BTN_RIGHT ||
                                       event.code == BTN_MIDDLE);
            if (event.type == EV_SYN) {
                virtual_keyboard.emit(event);
                virtual_mouse.emit(event);
            } else if (event.type == EV_REL || mouse_button) {
                virtual_mouse.emit(event);
            } else {
                virtual_keyboard.emit(event);
            }
        },
        exclusive);
    std::vector<PhysicalDevice> devices;
    std::set<std::filesystem::path> attached_paths;
    int next_id = 1;
    auto next_scan = std::chrono::steady_clock::time_point{};

    const auto rescan = [&]() {
        std::vector<std::string> names;
        for (const auto& item : config.devices) names.push_back(item.name);
        for (const auto& info : discover_keyboards(names)) {
            if (attached_paths.count(info.path) != 0) continue;
            const DeviceConfig* device_config = find_config(config, info.name);
            if (!device_config) continue;
            try {
                PhysicalDevice device(info.path, info.name, next_id++, *device_config, exclusive);
                controller.add_device(device.id(), device.config());
                attached_paths.insert(info.path);
                std::cout << "Baglandi: " << info.name << " (" << info.path << ") - "
                          << trigger_name(device_config->trigger) << '\n';
                devices.push_back(std::move(device));
            } catch (const std::exception& error) {
                std::cerr << "[UYARI] " << error.what() << '\n';
            }
        }
    };

    rescan();
    for (const auto& configured : config.devices) {
        const bool connected = std::any_of(devices.begin(), devices.end(),
                                           [&](const PhysicalDevice& device) {
                                               return device.name() == configured.name;
                                           });
        if (!connected) {
            std::cout << "[BEKLIYOR] Secili klavye bagli degil: " << configured.name
                      << " (baglaninca otomatik etkinlesecek)\n";
        }
    }
    next_scan = std::chrono::steady_clock::now() + std::chrono::seconds(2);

    while (g_running != 0) {
        const auto now = std::chrono::steady_clock::now();
        if (now >= next_scan) {
            rescan();
            next_scan = now + std::chrono::seconds(2);
        }

        std::vector<pollfd> poll_fds;
        poll_fds.reserve(devices.size() + 1);
        poll_fds.push_back({virtual_keyboard.fd(), POLLIN, 0});
        for (const auto& device : devices) {
            poll_fds.push_back({device.fd(), POLLIN, 0});
        }
        const int result = poll(poll_fds.data(), poll_fds.size(), 16);
        if (result < 0 && errno != EINTR) {
            throw std::runtime_error("poll basarisiz: " + std::string(std::strerror(errno)));
        }
        if (exclusive && !poll_fds.empty() && (poll_fds[0].revents & POLLIN)) {
            forward_led_events(virtual_keyboard.fd(), devices);
        }

        for (std::size_t index = devices.size(); index-- > 0;) {
            const short events = poll_fds[index + 1].revents;
            bool disconnected = (events & (POLLERR | POLLHUP | POLLNVAL)) != 0;
            if (events & POLLIN) {
                input_event raw{};
                while (true) {
                    const ssize_t count = read(devices[index].fd(), &raw, sizeof(raw));
                    if (count == static_cast<ssize_t>(sizeof(raw))) {
                        if (devices[index].requests_stop(raw)) {
                            std::cout << "Kapatma kombinasyonu algilandi.\n";
                            g_running = 0;
                        }
                        controller.handle_event(devices[index].id(),
                                                {raw.type, raw.code, raw.value});
                        if (g_running == 0) break;
                    } else if (count == 0 || (count < 0 && errno == ENODEV)) {
                        disconnected = true;
                        break;
                    } else if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                        break;
                    } else if (count < 0 && errno == EINTR) {
                        continue;
                    } else {
                        disconnected = true;
                        break;
                    }
                }
            }
            if (disconnected) {
                std::cout << "Ayrildi: " << devices[index].name() << '\n';
                controller.remove_device(devices[index].id());
                attached_paths.erase(devices[index].path());
                devices.erase(devices.begin() + static_cast<std::ptrdiff_t>(index));
            }
        }
        controller.tick(std::chrono::steady_clock::now());
    }

    controller.release_all();
    std::cout << "Guvenli bicimde durduruldu.\n";
    return 0;
}

} // namespace km

extern "C" void keyboard_mouse_signal_handler(int) {
    km::g_running = 0;
}
