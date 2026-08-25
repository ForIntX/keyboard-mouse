#include "keyboard_mouse/controller.hpp"

#include <linux/input-event-codes.h>

#include <algorithm>
#include <cmath>

namespace km {

Controller::Controller(Config config, Emit emit, bool forward_keyboard)
    : config_(std::move(config)), emit_(std::move(emit)),
      forward_keyboard_(forward_keyboard) {}

void Controller::add_device(int id, const DeviceConfig& config) {
    remove_device(id);
    DeviceState state;
    state.config = config;
    devices_.emplace(id, std::move(state));
}

void Controller::remove_device(int id) {
    const auto iterator = devices_.find(id);
    if (iterator == devices_.end()) {
        return;
    }
    auto& state = iterator->second;
    end_trigger(state);
    for (const int code : state.forwarded_down) {
        auto count = key_refcounts_.find(code);
        if (count != key_refcounts_.end() && --count->second <= 0) {
            emit_key(code, 0);
            key_refcounts_.erase(count);
        }
    }
    if (!state.forwarded_down.empty()) {
        emit_sync();
    }
    devices_.erase(iterator);
}

bool Controller::trigger_active(const DeviceState& state) const {
    if (state.config.trigger == TriggerKind::CapsLock) {
        return state.down.count(KEY_CAPSLOCK) != 0 && !state.caps_forwarded;
    }
    const bool ctrl = state.down.count(KEY_LEFTCTRL) != 0 ||
                      state.down.count(KEY_RIGHTCTRL) != 0;
    return state.config.fn_code >= 0 && state.down.count(state.config.fn_code) != 0 &&
           ctrl && state.ctrl_suppressed && !state.fn_forwarded;
}

bool Controller::is_command(const DeviceState& state, int code) const {
    return code == KEY_LEFT || code == KEY_RIGHT || code == KEY_UP || code == KEY_DOWN ||
           button_for_command(state, code) != 0;
}

int Controller::button_for_command(const DeviceState& state, int code) const {
    // Kalibrasyonda secilen fiziksel tusun yaninda hem ana sayi satirini hem de
    // sayisal tus takimini kabul et. Num Lock durumu evdev kodunu degistirmez.
    // Standart +/- kodlarini once kontrol etmek eski ayar dosyalarini da yeni
    // (+ sol, - sag) duzenine otomatik olarak tasir.
    if (code == KEY_EQUAL || code == KEY_KPPLUS) {
        return BTN_LEFT;
    }
    if (code == KEY_MINUS || code == KEY_KPMINUS) {
        return BTN_RIGHT;
    }
    if (code == KEY_0 || code == KEY_KP0) {
        return BTN_MIDDLE;
    }
    if (code == state.config.left_click_code) return BTN_LEFT;
    if (code == state.config.right_click_code) return BTN_RIGHT;
    if (code == state.config.middle_click_code) return BTN_MIDDLE;
    return 0;
}

void Controller::emit_key(int code, int value) {
    emit_({EV_KEY, static_cast<std::uint16_t>(code), value});
}

void Controller::emit_sync() {
    emit_({EV_SYN, SYN_REPORT, 0});
}

void Controller::forward_key(DeviceState& state, int code, int value) {
    // Guvenli (non-exclusive) modda fiziksel klavye olaylari zaten dogrudan
    // masaustune gider. Tekrar uinput'a yazmak harfleri cift basar.
    if (!forward_keyboard_) {
        return;
    }
    if (value == 1) {
        if (state.forwarded_down.insert(code).second && ++key_refcounts_[code] == 1) {
            emit_key(code, 1);
        }
    } else if (value == 0) {
        if (state.forwarded_down.erase(code) != 0) {
            auto count = key_refcounts_.find(code);
            if (count != key_refcounts_.end() && --count->second <= 0) {
                emit_key(code, 0);
                key_refcounts_.erase(count);
            }
        }
    } else if (state.forwarded_down.count(code) != 0) {
        emit_key(code, value);
    }
}

void Controller::forward_caps_if_needed(DeviceState& state) {
    if (state.config.trigger == TriggerKind::CapsLock &&
        state.down.count(KEY_CAPSLOCK) != 0 && !state.caps_forwarded &&
        !state.trigger_used) {
        forward_key(state, KEY_CAPSLOCK, 1);
        state.caps_forwarded = true;
    }
}

void Controller::forward_fn_ctrl_if_needed(DeviceState& state) {
    if (state.config.trigger != TriggerKind::FnCtrl || state.trigger_used) {
        return;
    }
    if (state.down.count(state.config.fn_code) != 0 && !state.fn_forwarded) {
        forward_key(state, state.config.fn_code, 1);
        state.fn_forwarded = true;
    }
    if (state.ctrl_suppressed) {
        if (state.down.count(KEY_LEFTCTRL) != 0) forward_key(state, KEY_LEFTCTRL, 1);
        if (state.down.count(KEY_RIGHTCTRL) != 0) forward_key(state, KEY_RIGHTCTRL, 1);
        state.ctrl_suppressed = false;
    }
}

void Controller::emit_button(int code, int value) {
    if (value == 1) {
        if (++button_refcounts_[code] == 1) {
            emit_key(code, 1);
        }
    } else {
        auto count = button_refcounts_.find(code);
        if (count != button_refcounts_.end() && --count->second <= 0) {
            emit_key(code, 0);
            button_refcounts_.erase(count);
        }
    }
}

void Controller::begin_command(DeviceState& state, int code) {
    state.trigger_used = true;
    state.swallowed_until_release.insert(code);
    if (code == KEY_LEFT || code == KEY_RIGHT || code == KEY_UP || code == KEY_DOWN) {
        state.active_directions.insert(code);
        return;
    }

    const int button = button_for_command(state, code);
    if (state.active_buttons.emplace(code, button).second) {
        emit_button(button, 1);
    }
}

void Controller::end_command(DeviceState& state, int code) {
    state.active_directions.erase(code);
    const auto button = state.active_buttons.find(code);
    if (button != state.active_buttons.end()) {
        emit_button(button->second, 0);
        state.active_buttons.erase(button);
    }
}

void Controller::end_trigger(DeviceState& state) {
    state.active_directions.clear();
    for (const auto& [key, button] : state.active_buttons) {
        (void)key;
        emit_button(button, 0);
    }
    state.active_buttons.clear();
}

void Controller::handle_key(DeviceState& state, const OutputEvent& event) {
    const int code = event.code;
    const int value = event.value;
    const bool was_down = state.down.count(code) != 0;

    if (value == 1) {
        state.down.insert(code);
    } else if (value == 0) {
        state.down.erase(code);
    }

    if (value == 0 && state.swallowed_until_release.erase(code) != 0) {
        end_command(state, code);
        return;
    }

    if (state.config.trigger == TriggerKind::CapsLock && code == KEY_CAPSLOCK) {
        if (value == 1) {
            state.trigger_used = false;
            state.caps_forwarded = false;
        } else if (value == 0) {
            end_trigger(state);
            if (state.caps_forwarded) {
                forward_key(state, KEY_CAPSLOCK, 0);
            } else if (!state.trigger_used) {
                forward_key(state, KEY_CAPSLOCK, 1);
                emit_sync();
                forward_key(state, KEY_CAPSLOCK, 0);
            } else if (!forward_keyboard_) {
                // Guvenli modda fiziksel Caps olayi masaustune zaten ulasti.
                // Fare komutundan sonra kilit durumunu ikinci bir Caps tikiyla
                // eski haline getir.
                emit_key(KEY_CAPSLOCK, 1);
                emit_sync();
                emit_key(KEY_CAPSLOCK, 0);
            }
            state.trigger_used = false;
            state.caps_forwarded = false;
        }
        return;
    }

    if (state.config.trigger == TriggerKind::FnCtrl) {
        if (code == state.config.fn_code) {
            if (value == 1) {
                state.trigger_used = false;
                state.fn_forwarded = false;
            } else if (value == 0) {
                end_trigger(state);
                if (state.fn_forwarded) {
                    forward_key(state, code, 0);
                } else if (!state.trigger_used) {
                    forward_key(state, code, 1);
                    forward_key(state, code, 0);
                }
                state.fn_forwarded = false;
                const bool ctrl_down = state.down.count(KEY_LEFTCTRL) != 0 ||
                                       state.down.count(KEY_RIGHTCTRL) != 0;
                if (!ctrl_down) state.trigger_used = false;
            }
            return;
        }
        if ((code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) &&
            ((value == 1 && state.down.count(state.config.fn_code) != 0) ||
             state.ctrl_suppressed)) {
            if (value == 1) {
                state.ctrl_suppressed = true;
            } else if (value == 0) {
                end_trigger(state);
                if (!state.trigger_used) {
                    forward_key(state, code, 1);
                    forward_key(state, code, 0);
                }
                state.ctrl_suppressed = false;
                if (state.down.count(state.config.fn_code) == 0) state.trigger_used = false;
            }
            return;
        }
    }

    if ((value == 1 || value == 2) && trigger_active(state) && is_command(state, code)) {
        if (!was_down || value == 1) {
            begin_command(state, code);
        }
        return;
    }

    if (value == 0 && (code == KEY_LEFTCTRL || code == KEY_RIGHTCTRL) &&
        state.trigger_used) {
        end_trigger(state);
    }

    const bool harmless_modifier = code == KEY_LEFTSHIFT || code == KEY_RIGHTSHIFT ||
                                   code == KEY_LEFTALT || code == KEY_RIGHTALT ||
                                   code == KEY_LEFTMETA || code == KEY_RIGHTMETA;
    if (!harmless_modifier) {
        forward_caps_if_needed(state);
        forward_fn_ctrl_if_needed(state);
    }
    forward_key(state, code, value);
}

void Controller::handle_event(int id, const OutputEvent& event) {
    const auto iterator = devices_.find(id);
    if (iterator == devices_.end()) {
        return;
    }
    if (event.type == EV_KEY) {
        handle_key(iterator->second, event);
    } else if (event.type == EV_SYN && event.code == SYN_REPORT) {
        emit_(event);
    }
}

void Controller::tick(std::chrono::steady_clock::time_point now) {
    int horizontal = 0;
    int vertical = 0;
    for (const auto& [id, state] : devices_) {
        (void)id;
        horizontal += state.active_directions.count(KEY_RIGHT) != 0;
        horizontal -= state.active_directions.count(KEY_LEFT) != 0;
        vertical += state.active_directions.count(KEY_DOWN) != 0;
        vertical -= state.active_directions.count(KEY_UP) != 0;
    }
    horizontal = std::clamp(horizontal, -1, 1);
    vertical = std::clamp(vertical, -1, 1);

    if (horizontal == 0 && vertical == 0) {
        movement_started_ = {};
        last_tick_ = {};
        x_remainder_ = 0;
        y_remainder_ = 0;
        return;
    }
    if (movement_started_ == std::chrono::steady_clock::time_point{}) {
        movement_started_ = now;
        last_tick_ = now;
        return;
    }

    double dt = std::chrono::duration<double>(now - last_tick_).count();
    last_tick_ = now;
    dt = std::clamp(dt, 0.0, 0.05);
    const double held = std::chrono::duration<double>(now - movement_started_).count();
    const double progress = std::clamp(held / config_.acceleration_seconds, 0.0, 1.0);
    const double speed = config_.start_speed +
                         (config_.max_speed - config_.start_speed) * progress;
    const double diagonal = horizontal != 0 && vertical != 0 ? std::sqrt(0.5) : 1.0;
    x_remainder_ += horizontal * speed * diagonal * dt;
    y_remainder_ += vertical * speed * diagonal * dt;
    const int dx = static_cast<int>(x_remainder_);
    const int dy = static_cast<int>(y_remainder_);
    x_remainder_ -= dx;
    y_remainder_ -= dy;
    if (dx != 0) {
        emit_({EV_REL, REL_X, dx});
    }
    if (dy != 0) {
        emit_({EV_REL, REL_Y, dy});
    }
    if (dx != 0 || dy != 0) {
        emit_sync();
    }
}

void Controller::release_all() {
    std::vector<int> ids;
    ids.reserve(devices_.size());
    for (const auto& [id, state] : devices_) {
        (void)state;
        ids.push_back(id);
    }
    for (const int id : ids) {
        remove_device(id);
    }
    for (const auto& [button, count] : button_refcounts_) {
        (void)count;
        emit_key(button, 0);
    }
    button_refcounts_.clear();
    emit_sync();
}

} // namespace km
