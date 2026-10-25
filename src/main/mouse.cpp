// Mouse aiming (include/snp_mouse.h), and the stick's Analog Aim and
// Stick Sensitivity.
//
// The reticle is two floats in the struct func_800AB924 moves it with ($s2 +
// 0x28 and + 0x2C): its offset from the screen centre in 320 x 240 screen
// units, y up. Each frame the game adds a velocity from the stick, then clamps
// it (src/game/widescreen.cpp widens the clamp in Expand). Just before the
// velocity is added, the mouse moves it (snp.jp.toml), only on frames the mouse
// moved, so a controller aims as before in between.
//
// Mouse buttons are bound like keys on the Controls tab
// (lib-patches/RecompFrontend/0008, defaults in src/main/input_defaults.cpp).
//
// On Android a finger aims the same way (Touch Aim, src/android/touch_controls.cpp):
// Point puts the reticle under it like Absolute, Drag moves it like Relative.
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
#include "snp_config.h"

extern SDL_Window* window;

namespace {
    std::mutex motion_mutex;
    float relative_x = 0.0f; // pixels moved since the game last took them
    float relative_y = 0.0f;
    float pointer_x = 0.0f;  // the pointer in the window, in pixels
    float pointer_y = 0.0f;
    bool pointer_moved = false;
    int window_w = 1280;
    int window_h = 720;

    // The aiming finger, in the touch view's pixels (guarded by motion_mutex).
    struct {
        float relative_x = 0.0f;
        float relative_y = 0.0f;
        float x = 0.0f;
        float y = 0.0f;
        float last_x = 0.0f;
        float last_y = 0.0f;
        bool moved = false;
        int view_w = 1280;
        int view_h = 720;
    } touch;
    // The Touch Controls tab's defaults until its settings load.
    std::atomic<snp::mouse::TouchAim> touch_aim{ snp::mouse::TouchAim::Point };
    std::atomic<float> touch_speed{ 100.0f };

    int SDLCALL watch_events(void*, SDL_Event* event) {
        switch (event->type) {
            case SDL_MOUSEMOTION: {
                if (recompinput::game_input_disabled()) {
                    break;
                }
#if defined(__ANDROID__)
                // SDL's mouse made from touches; the touch view aims with fingers itself.
                if (event->motion.which == SDL_TOUCH_MOUSEID) {
                    break;
                }
#endif
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

    uint8_t read_byte(uint8_t* rdram, uint32_t addr) {
        return rdram[(addr ^ 3) & 0x1FFFFFFF]; // RDRAM is stored in 32-bit words
    }

    // Lock-on mode (the aim mode's flag 4 at *0x800DA978): func_800AB924 calls
    // func_800AB31C, which moves the reticle with the stick until the stick
    // is pushed hard (a magnitude of 48 of 64), then locks onto the nearest
    // target that way and steers the reticle onto it (the reticle's byte 2
    // set) until it's lost. The reticle's flag 0x20 overrides it.
    bool lock_on_mode(uint8_t* rdram, uint32_t reticle) {
        uint32_t aim_mode = *reinterpret_cast<uint32_t*>(rdram + 0xDA978);
        return !(read_byte(rdram, reticle) & 0x20) && (read_byte(rdram, aim_mode) & 4);
    }

    // The attract mode's How to Play plays back recorded input (func_800A8638
    // reads it while this byte is set): the reticle is left to the recording,
    // or the demo goes off its script.
    bool demo_playing(uint8_t* rdram) {
        return read_byte(rdram, 0x800D4F00) != 0;
    }

    bool locked_on(uint8_t* rdram, uint32_t reticle) {
        return lock_on_mode(rdram, reticle) && read_byte(rdram, reticle + 2) != 0;
    }

    // Window pixels per screen unit: the picture is 240 units high, or 320
    // wide in a window narrower than 4:3 (Expand only widens it).
    float screen_unit(int w, int h) {
        return (w * 3 >= h * 4) ? h / 240.0f : w / 320.0f;
    }

    // Stick Sensitivity: the reticle moves by its velocity times the
    // sensitivity (the game adds the velocity next; this adds the rest), so
    // the game's own speeds and stick handling stay as they are. Not while
    // lock-on steers it (func_800AB924 calls func_800AB31C instead of the
    // stick's func_800AB1C4: the reticle's flag 0x20 clear and the aim mode's
    // flag 4 set).
    void apply_stick_sensitivity(uint8_t* rdram, uint32_t reticle) {
        float sensitivity = snp::config::get_stick_sensitivity() / 100.0f;
        if (sensitivity == 1.0f) {
            return;
        }
        if (lock_on_mode(rdram, reticle)) {
            return;
        }
        for (uint32_t axis = 0; axis < 2; axis++) {
            float velocity = read_float(rdram, reticle + 0x58 + axis * 4);
            float position = read_float(rdram, reticle + 0x28 + axis * 4) + velocity * (sensitivity - 1.0f);
            if (std::isfinite(position)) {
                write_float(rdram, reticle + 0x28 + axis * 4, position);
            }
        }
    }
}

void snp::mouse::set_touch_aim(TouchAim aim) {
    touch_aim.store(aim);
}

void snp::mouse::set_touch_aim_speed(float speed_percent) {
    touch_speed.store(speed_percent);
}

snp::mouse::TouchAim snp::mouse::get_touch_aim() {
    return touch_aim.load();
}

void snp::mouse::touch_aim_down(float x, float y, int view_w, int view_h) {
    std::lock_guard lock(motion_mutex);
    touch.x = touch.last_x = x;
    touch.y = touch.last_y = y;
    touch.view_w = view_w;
    touch.view_h = view_h;
    // Point puts the reticle under the finger at once; Drag waits for it to move.
    if (touch_aim.load() == TouchAim::Point) {
        touch.moved = true;
    }
}

void snp::mouse::touch_aim_move(float x, float y) {
    std::lock_guard lock(motion_mutex);
    touch.relative_x += x - touch.last_x;
    touch.relative_y += y - touch.last_y;
    touch.x = touch.last_x = x;
    touch.y = touch.last_y = y;
    touch.moved = true;
}

void snp::mouse::init() {
    SDL_AddEventWatch(watch_events, nullptr);
    recompinput::set_game_mouse_callback(game_mouse);
}

// func_800AB924, before the reticle's velocity is added: the reticle struct.
extern "C" void snp_mouse_reticle(uint8_t* rdram, uint32_t reticle) {
    snp::config::MouseAim aim = snp::config::get_mouse_aim();
    snp::mouse::TouchAim finger_aim = touch_aim.load();

    float dx, dy, px, py, tdx, tdy, tx, ty;
    int w, h, tw, th;
    bool moved, touched;
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
        tdx = touch.relative_x;
        tdy = touch.relative_y;
        touch.relative_x = touch.relative_y = 0.0f;
        tx = touch.x;
        ty = touch.y;
        touched = touch.moved;
        touch.moved = false;
        tw = touch.view_w;
        th = touch.view_h;
    }
    if (demo_playing(rdram)) {
        return;
    }
    // A finger wins over the mouse in a frame both moved in.
    bool use_touch = finger_aim != snp::mouse::TouchAim::Off && touched;
    bool use_mouse = !use_touch && aim != snp::config::MouseAim::Off && moved;
    if (!use_touch && !use_mouse) {
        apply_stick_sensitivity(rdram, reticle);
        return;
    }

    // Locked on, the game steers the reticle; the mouse picks targets
    // (snp_aim_stick).
    if (locked_on(rdram, reticle)) {
        return;
    }
    float x, y;
    if (use_touch) {
        float unit = screen_unit(tw, th);
        if (finger_aim == snp::mouse::TouchAim::Drag) {
            // At 100% the reticle moves with the finger.
            float scale = touch_speed.load() / 100.0f / unit;
            x = read_float(rdram, reticle + 0x28) + tdx * scale;
            y = read_float(rdram, reticle + 0x2C) - tdy * scale;
        }
        else {
            x = (tx - tw * 0.5f) / unit;
            y = (th * 0.5f - ty) / unit;
        }
    }
    else {
        float unit = screen_unit(w, h);
        if (aim == snp::config::MouseAim::Relative) {
            float scale = snp::config::get_mouse_sensitivity() / 50.0f / unit;
            x = read_float(rdram, reticle + 0x28) + dx * scale;
            y = read_float(rdram, reticle + 0x2C) - dy * scale;
        }
        else {
            x = (px - w * 0.5f) / unit;
            y = (h * 0.5f - py) / unit;
        }
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

// func_800AB924, with the stick's offsets as read and as the game stepped them
// (x, y): with Analog Aim on, the offsets as read, within the same -64 .. 64.
// In lock-on mode with the stick at rest, a mouse flick (a fast move since
// the last frame) is a full push of the stick its way, so the game locks onto
// the next target in that direction, as it does for a flick of the stick.
extern "C" void snp_aim_stick(uint8_t* rdram, uint32_t reticle, int32_t raw_x, int32_t raw_y, int32_t* x, int32_t* y) {
    if (demo_playing(rdram)) {
        return;
    }
    if (snp::config::get_option(snp::config::Option::AnalogAim)) {
        *x = std::clamp(raw_x, -64, 64);
        *y = std::clamp(raw_y, -64, 64);
    }
    bool mouse_aims = snp::config::get_mouse_aim() != snp::config::MouseAim::Off;
    bool finger_aims = touch_aim.load() != snp::mouse::TouchAim::Off;
    if ((!mouse_aims && !finger_aims) || !lock_on_mode(rdram, reticle) || *x != 0 || *y != 0) {
        return;
    }
    // In screen units; a finger's swipe flicks the same way.
    float dx = 0.0f, dy = 0.0f;
    {
        std::lock_guard lock(motion_mutex);
        if (mouse_aims) {
            float unit = screen_unit(window_w, window_h);
            dx += relative_x / unit;
            dy += relative_y / unit;
        }
        if (finger_aims) {
            float unit = screen_unit(touch.view_w, touch.view_h);
            dx += touch.relative_x / unit;
            dy += touch.relative_y / unit;
        }
    }
    constexpr float flick = 8.0f; // screen units in a frame
    float distance = std::sqrt(dx * dx + dy * dy);
    if (distance < flick) {
        return;
    }
    *x = (int32_t)std::lround(dx / distance * 64.0f);
    *y = (int32_t)std::lround(-dy / distance * 64.0f);
}
