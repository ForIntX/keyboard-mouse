#pragma once

#include "keyboard_mouse/config.hpp"

#include <chrono>
#include <cstdint>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace km {

struct OutputEvent {
    std::uint16_t type;
    std::uint16_t code;
    std::int32_t value;
};

using Emit = std::function<void(const OutputEvent&)>;

class Controller {
public:
    explicit Controller(Config config, Emit emit, bool forward_keyboard = true);

    void add_device(int id, const DeviceConfig& config);
    void remove_device(int id);
    void handle_event(int id, const OutputEvent& event);
    void tick(std::chrono::steady_clock::time_point now);
    void release_all();

private:
    struct DeviceState {
        DeviceConfig config;
        std::unordered_set<int> down;
        std::unordered_set<int> swallowed_until_release;
        bool trigger_used = false;
        bool caps_forwarded = false;
        bool ctrl_suppressed = false;
        bool fn_forwarded = false;
        std::unordered_map<int, int> active_buttons;
        std::unordered_set<int> active_directions;
        std::unordered_set<int> forwarded_down;
    };

    bool trigger_active(const DeviceState& state) const;
    bool is_command(const DeviceState& state, int code) const;
    int button_for_command(const DeviceState& state, int code) const;
    void handle_key(DeviceState& state, const OutputEvent& event);
    void begin_command(DeviceState& state, int code);
    void end_command(DeviceState& state, int code);
    void end_trigger(DeviceState& state);
    void forward_caps_if_needed(DeviceState& state);
    void forward_fn_ctrl_if_needed(DeviceState& state);
    void forward_key(DeviceState& state, int code, int value);
    void emit_key(int code, int value);
    void emit_sync();
    void emit_button(int code, int value);

    Config config_;
    Emit emit_;
    bool forward_keyboard_ = true;
    std::unordered_map<int, DeviceState> devices_;
    std::unordered_map<int, int> button_refcounts_;
    std::unordered_map<int, int> key_refcounts_;
    std::chrono::steady_clock::time_point movement_started_{};
    std::chrono::steady_clock::time_point last_tick_{};
    double x_remainder_ = 0.0;
    double y_remainder_ = 0.0;
};

} // namespace km
