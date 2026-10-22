// Mouse aiming (include/snp_mouse.h), and the stick's analog aim.
//
// The reticle is two floats in the struct func_800AB924 moves it with ($s2 +
// 0x28 and + 0x2C): its offset from the screen centre in 320 x 240 screen
// units, y up. Each frame the game adds a velocity from the stick, then clamps
// it (src/game/widescreen.cpp widens the clamp in Expand). Just before the
// velocity is added, the mouse moves it (snp.jp.toml), only on frames the mouse
// moved, so a controller aims as before in between.
//
// Mouse buttons can't be bound in RecompFrontend's controls menu, so they're
// fixed: left click Z (fire), right click R (jump), middle click A (aim mode).
#include "snp_mouse.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstring>
#include <mutex>

#ifdef _WIN32
#include "SDL.h"
#else
#include "SDL2/SDL.h"
#endif

#include "recompinput/input_events.h"
#include "recompinput/input_state.h"
#include "recompinput/profiles.h"
#include "snp_config.h"

extern SDL_Window* window;

namespace {
    constexpr uint16_t BTN_A = 0x8000;
    constexpr uint16_t BTN_Z = 0x2000;
    constexpr uint16_t BTN_R = 0x0010;

    std::mutex motion_mutex;
    float relative_x = 0.0f; // pixels moved since the game last took them
    float relative_y = 0.0f;
    float pointer_x = 0.0f;  // the pointer in the window, in pixels
    float pointer_y = 0.0f;
    bool pointer_moved = false;
    int window_w = 1280;
    int window_h = 720;

    std::atomic<uint32_t> mouse_buttons = 0;

    int SDLCALL watch_events(void*, SDL_Event* event) {
        switch (event->type) {
            case SDL_MOUSEMOTION: {
                if (recompinput::game_input_disabled()) {
                    break;
                }
                int w = 0, h = 0;
                if (window != nullptr) {
                    SDL_GetWindowSize(window, &w, &h);
                }
                std::lock_guard lock(motion_mutex);
                relative_x += (float)event->motion.xrel;
                relative_y += (float)event->motion.yrel;
                pointer_x = (float)event->motion.x;
                pointer_y = (float)event->motion.y;
                pointer_moved = true;
                if (w > 0 && h > 0) {
                    window_w = w;
                    window_h = h;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
                mouse_buttons.fetch_or(SDL_BUTTON(event->button.button));
                break;
            case SDL_MOUSEBUTTONUP:
                mouse_buttons.fetch_and(~SDL_BUTTON(event->button.button));
                break;
            default:
                break;
        }
        return 0;
    }

    recompinput::GameMouse game_mouse() {
        switch (snp::config::get_mouse_aim()) {
            case snp::config::MouseAim::Relative: return recompinput::GameMouse::Captured;
            case snp::config::MouseAim::Absolute: return recompinput::GameMouse::Hidden;
            default: return recompinput::GameMouse::Default;
        }
    }

    float read_float(uint8_t* rdram, uint32_t addr) {
        float f;
        std::memcpy(&f, rdram + (addr & 0x1FFFFFFF), sizeof(f));
        return f;
    }

    void write_float(uint8_t* rdram, uint32_t addr, float f) {
        std::memcpy(rdram + (addr & 0x1FFFFFFF), &f, sizeof(f));
    }
}

void snp::mouse::init() {
    SDL_AddEventWatch(watch_events, nullptr);
    recompinput::set_game_mouse_callback(game_mouse);
}

bool snp::mouse::get_input(int player, uint16_t* buttons, float* x, float* y) {
    bool connected = recompinput::profiles::get_n64_input(player, buttons, x, y);
    if (player == 0 && snp::config::get_mouse_aim() != snp::config::MouseAim::Off && !recompinput::game_input_disabled()) {
        uint32_t held = mouse_buttons.load();
        if (held & SDL_BUTTON_LMASK) *buttons |= BTN_Z;
        if (held & SDL_BUTTON_RMASK) *buttons |= BTN_R;
        if (held & SDL_BUTTON_MMASK) *buttons |= BTN_A;
    }
    return connected;
}

// func_800AB924, before the reticle's velocity is added: the reticle struct.
extern "C" void snp_mouse_reticle(uint8_t* rdram, uint32_t reticle) {
    snp::config::MouseAim aim = snp::config::get_mouse_aim();
    if (aim == snp::config::MouseAim::Off) {
        return;
    }

    float dx, dy, px, py;
    int w, h;
    bool moved;
    {
        std::lock_guard lock(motion_mutex);
        dx = relative_x;
        dy = relative_y;
        relative_x = relative_y = 0.0f;
        px = pointer_x;
        py = pointer_y;
        moved = pointer_moved;
        pointer_moved = false;
        w = window_w;
        h = window_h;
    }
    if (!moved) {
        return;
    }

    // Window pixels per screen unit: the picture is 240 units high, or 320
    // wide in a window narrower than 4:3 (Expand only widens it).
    float unit = (w * 3 >= h * 4) ? h / 240.0f : w / 320.0f;
    float x, y;
    if (aim == snp::config::MouseAim::Relative) {
        float scale = snp::config::get_mouse_sensitivity() / 50.0f / unit;
        x = read_float(rdram, reticle + 0x28) + dx * scale;
        y = read_float(rdram, reticle + 0x2C) - dy * scale;
    }
    else {
        x = (px - w * 0.5f) / unit;
        y = (h * 0.5f - py) / unit;
    }
    if (!std::isfinite(x) || !std::isfinite(y)) {
        return;
    }
    write_float(rdram, reticle + 0x28, x);
    write_float(rdram, reticle + 0x2C, y);
    // The game clamps the position next; the stick's velocity is let go.
    write_float(rdram, reticle + 0x58, 0.0f);
    write_float(rdram, reticle + 0x5C, 0.0f);
}

// func_800AB924: the stick's offset as the game stepped it, and as read; with
// Analog Aim on, the offset as read, within the same -64 .. 64.
extern "C" int32_t snp_analog_aim(int32_t stepped, int32_t raw) {
    if (!snp::config::get_option(snp::config::Option::AnalogAim)) {
        return stepped;
    }
    return std::clamp(raw, -64, 64);
}
