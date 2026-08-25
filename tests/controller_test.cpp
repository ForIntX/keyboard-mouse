#include "keyboard_mouse/controller.hpp"

#include <linux/input-event-codes.h>

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

using Clock = std::chrono::steady_clock;

void require(bool condition, const std::string& message) {
    if (!condition) {
        std::cerr << "FAILED: " << message << '\n';
        std::exit(1);
    }
}

int count_event(const std::vector<km::OutputEvent>& events, int type, int code, int value) {
    return static_cast<int>(std::count_if(events.begin(), events.end(), [&](const auto& event) {
        return event.type == type && event.code == code && event.value == value;
    }));
}

km::DeviceConfig caps_config() {
    km::DeviceConfig config;
    config.name = "test";
    config.trigger = km::TriggerKind::CapsLock;
    config.left_click_code = KEY_EQUAL;
    config.right_click_code = KEY_MINUS;
    config.middle_click_code = KEY_0;
    return config;
}

void key(km::Controller& controller, int code, int value) {
    controller.handle_event(1, {EV_KEY, static_cast<std::uint16_t>(code), value});
    controller.handle_event(1, {EV_SYN, SYN_REPORT, 0});
}

void test_caps_tap_is_preserved() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_CAPSLOCK, 0);
    require(count_event(events, EV_KEY, KEY_CAPSLOCK, 1) == 1, "Caps tap press forwarded");
    require(count_event(events, EV_KEY, KEY_CAPSLOCK, 0) == 1, "Caps tap release forwarded");
}

void test_caps_command_is_consumed_and_moves() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    config.start_speed = 180;
    config.max_speed = 1080;
    config.acceleration_seconds = 1.2;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    const auto start = Clock::now();
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_RIGHT, 1);
    controller.tick(start);
    controller.tick(start + std::chrono::milliseconds(100));
    key(controller, KEY_RIGHT, 0);
    key(controller, KEY_CAPSLOCK, 0);
    require(count_event(events, EV_KEY, KEY_CAPSLOCK, 1) == 0, "command does not toggle Caps");
    require(count_event(events, EV_KEY, KEY_RIGHT, 1) == 0, "arrow is consumed");
    require(std::any_of(events.begin(), events.end(), [](const auto& event) {
        return event.type == EV_REL && event.code == REL_X && event.value > 0;
    }), "right movement emitted");
}

void test_click_hold_and_trigger_release() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_EQUAL, 1);
    key(controller, KEY_CAPSLOCK, 0);
    key(controller, KEY_EQUAL, 0);
    require(count_event(events, EV_KEY, BTN_LEFT, 1) == 1, "left button pressed");
    require(count_event(events, EV_KEY, BTN_LEFT, 0) == 1, "left button safely released");
    require(count_event(events, EV_KEY, KEY_EQUAL, 1) == 0, "click key consumed");
}

void test_normal_keys_and_unplug_release() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    key(controller, KEY_A, 1);
    controller.remove_device(1);
    require(count_event(events, EV_KEY, KEY_A, 1) == 1, "normal key forwarded");
    require(count_event(events, EV_KEY, KEY_A, 0) == 1, "held key released on unplug");
}

void test_fn_ctrl_trigger() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    auto device = caps_config();
    device.trigger = km::TriggerKind::FnCtrl;
    device.fn_code = KEY_FN;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, device);
    key(controller, KEY_FN, 1);
    key(controller, KEY_LEFTCTRL, 1);
    key(controller, KEY_0, 1);
    key(controller, KEY_0, 0);
    key(controller, KEY_LEFTCTRL, 0);
    key(controller, KEY_FN, 0);
    require(count_event(events, EV_KEY, BTN_MIDDLE, 1) == 1, "Fn+Ctrl middle press");
    require(count_event(events, EV_KEY, BTN_MIDDLE, 0) == 1, "Fn+Ctrl middle release");
    require(count_event(events, EV_KEY, KEY_LEFTCTRL, 1) == 0, "used Ctrl is consumed");
    require(count_event(events, EV_KEY, KEY_FN, 1) == 0, "used Fn is consumed");
}

void test_fn_ctrl_release_order_does_not_leak_ctrl() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    auto device = caps_config();
    device.trigger = km::TriggerKind::FnCtrl;
    device.fn_code = KEY_FN;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, device);
    key(controller, KEY_FN, 1);
    key(controller, KEY_LEFTCTRL, 1);
    key(controller, KEY_RIGHT, 1);
    key(controller, KEY_RIGHT, 0);
    key(controller, KEY_FN, 0);
    key(controller, KEY_LEFTCTRL, 0);
    require(count_event(events, EV_KEY, KEY_LEFTCTRL, 1) == 0,
            "Ctrl stays consumed when Fn is released first");
}

void test_unused_fn_ctrl_chord_is_forwarded() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    auto device = caps_config();
    device.trigger = km::TriggerKind::FnCtrl;
    device.fn_code = KEY_FN;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, device);
    key(controller, KEY_FN, 1);
    key(controller, KEY_LEFTCTRL, 1);
    key(controller, KEY_C, 1);
    key(controller, KEY_C, 0);
    key(controller, KEY_LEFTCTRL, 0);
    key(controller, KEY_FN, 0);
    require(count_event(events, EV_KEY, KEY_FN, 1) == 1, "unused Fn is forwarded");
    require(count_event(events, EV_KEY, KEY_LEFTCTRL, 1) == 1, "unused Ctrl is forwarded");
    require(count_event(events, EV_KEY, KEY_C, 1) == 1, "non-command key is forwarded");
}

void test_shifted_click_does_not_toggle_caps() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_LEFTSHIFT, 1);
    key(controller, KEY_EQUAL, 1);
    key(controller, KEY_EQUAL, 0);
    key(controller, KEY_LEFTSHIFT, 0);
    key(controller, KEY_CAPSLOCK, 0);
    require(count_event(events, EV_KEY, BTN_LEFT, 1) == 1, "shifted plus clicks left");
    require(count_event(events, EV_KEY, KEY_CAPSLOCK, 1) == 0,
            "shifted command does not toggle Caps");
}

void test_numpad_click_aliases() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); });
    controller.add_device(1, caps_config());
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_KPMINUS, 1);
    key(controller, KEY_KPMINUS, 0);
    key(controller, KEY_KPPLUS, 1);
    key(controller, KEY_KPPLUS, 0);
    key(controller, KEY_KP0, 1);
    key(controller, KEY_KP0, 0);
    key(controller, KEY_CAPSLOCK, 0);
    require(count_event(events, EV_KEY, BTN_RIGHT, 1) == 1, "numpad minus clicks right");
    require(count_event(events, EV_KEY, BTN_LEFT, 1) == 1, "numpad plus clicks left");
    require(count_event(events, EV_KEY, BTN_MIDDLE, 1) == 1, "numpad zero clicks middle");
}

void test_safe_mode_never_reemits_normal_keyboard() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); }, false);
    controller.add_device(1, caps_config());
    key(controller, KEY_A, 1);
    key(controller, KEY_A, 0);
    require(count_event(events, EV_KEY, KEY_A, 1) == 0,
            "safe mode does not duplicate normal keys");

    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_EQUAL, 1);
    key(controller, KEY_EQUAL, 0);
    key(controller, KEY_CAPSLOCK, 0);
    require(count_event(events, EV_KEY, BTN_LEFT, 1) == 1,
            "safe mode still emits mouse clicks");
    require(count_event(events, EV_KEY, KEY_CAPSLOCK, 1) == 1,
            "safe mode restores Caps state after a command");
}

void test_exclusive_keyboard_recovers_after_mouse_command() {
    std::vector<km::OutputEvent> events;
    km::Config config;
    km::Controller controller(config, [&](const auto& event) { events.push_back(event); }, true);
    controller.add_device(1, caps_config());
    key(controller, KEY_CAPSLOCK, 1);
    key(controller, KEY_RIGHT, 1);
    key(controller, KEY_RIGHT, 0);
    key(controller, KEY_CAPSLOCK, 0);
    key(controller, KEY_A, 1);
    key(controller, KEY_A, 0);
    key(controller, KEY_LEFTCTRL, 1);
    key(controller, KEY_C, 1);
    key(controller, KEY_C, 0);
    key(controller, KEY_LEFTCTRL, 0);
    require(count_event(events, EV_KEY, KEY_A, 1) == 1,
            "normal typing works after a mouse command");
    require(count_event(events, EV_KEY, KEY_LEFTCTRL, 1) == 1,
            "Ctrl is forwarded after a mouse command");
    require(count_event(events, EV_KEY, KEY_C, 1) == 1,
            "Ctrl+C is forwarded after a mouse command");
}

} // namespace

int main() {
    test_caps_tap_is_preserved();
    test_caps_command_is_consumed_and_moves();
    test_click_hold_and_trigger_release();
    test_normal_keys_and_unplug_release();
    test_fn_ctrl_trigger();
    test_fn_ctrl_release_order_does_not_leak_ctrl();
    test_unused_fn_ctrl_chord_is_forwarded();
    test_shifted_click_does_not_toggle_caps();
    test_numpad_click_aliases();
    test_safe_mode_never_reemits_normal_keyboard();
    test_exclusive_keyboard_recovers_after_mouse_command();
    std::cout << "All controller tests passed.\n";
    return 0;
}
