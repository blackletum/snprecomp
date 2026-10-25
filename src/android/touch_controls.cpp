// On-screen touch controls and touch aiming (Android), on top of descore's touch module
// (src/android/descore/touch), as BattleTanx's touch_controls.cpp is.
//
// Java (TouchControlsView) runs everything here on the UI thread: it forwards touches, the screen
// size and whether a gamepad is connected, and draws the buttons from descore_touch_get_shapes().
// While the game takes input every finger comes here: one that lands on a button goes to descore,
// the first other one aims (src/main/mouse.cpp, Touch Aim), so a finger can aim while others move,
// jump or fire. Aiming works whether or not the buttons show. The settings come from the Touch
// Controls tab (src/main/game_config.cpp), and the game thread reads the buttons (add_touch_input),
// both through atomics.
#include <jni.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>

extern "C" {
#include "descore/touch/descore_touch.h"
}

#include "snp_android.h"
#include "snp_mouse.h"
#include "recompinput/input_state.h"
#include "ultramodern/ultramodern.hpp"

namespace {
    // Buttons, in descore's button order; a held button sends its own index as its one key.
    enum Button {
        BUTTON_FIRE,  // Z
        BUTTON_JUMP,  // R
        BUTTON_AIM,   // A: the aim mode (lock-on / free)
        BUTTON_START,
        BUTTON_MENU,  // a tap button: opens the game's menu (Java sends Escape)
        BUTTON_COUNT
    };

    // libultra's controller button bits.
    constexpr uint16_t kZ = 0x2000, kR = 0x0010, kA = 0x8000, kStart = 0x1000;
    constexpr uint16_t kCUp = 0x0008, kCDown = 0x0004, kCLeft = 0x0002, kCRight = 0x0001;
    constexpr uint16_t kN64Bits[] = { kZ, kR, kA, kStart };

    const char* const kLabels[BUTTON_COUNT] = { "FIRE", "JUMP", "AIM", "START", "MENU" };

    std::atomic<uint32_t> held_buttons{ 0 };
    // The movement stick, as the C buttons the game moves the character with.
    std::atomic<uint32_t> held_move{ 0 };
    std::atomic<bool> aim_finger_fires{ false };

    // Settings, from the Touch Controls tab; Java lays the buttons out again when they change.
    std::atomic<snp::android::TouchButtons> setting_buttons{ snp::android::TouchButtons::Auto };
    std::atomic<bool> setting_fire_while_touching{ true };
    std::atomic<double> setting_size{ 100.0 };
    std::atomic<double> setting_opacity{ 30.0 };
    std::atomic<bool> settings_changed{ false };

    bool gamepad_connected = false;
    bool menu_tapped = false;
    int view_w = 0;
    int view_h = 0;

    // Who has each finger.
    enum class Owner : uint8_t { None, Buttons, Aim };
    Owner owners[DESCORE_TOUCH_MAX_POINTERS] = {};
    int aim_pointer = -1;

    // The descore Size step nearest to a size in percent.
    int scale_step_for(double percent) {
        int best = DESCORE_TOUCH_SCALE_DEFAULT_STEP;
        for (int i = 0; i < DESCORE_TOUCH_SCALE_NUM_STEPS; i++) {
            if (std::abs(kDescoreTouchScaleValues[i] * 100.0 - percent) <
                std::abs(kDescoreTouchScaleValues[best] * 100.0 - percent)) {
                best = i;
            }
        }
        return best;
    }

    void on_key(unsigned char key, int down) {
        if (key >= BUTTON_MENU) {
            return;
        }
        if (down) {
            held_buttons.fetch_or(kN64Bits[key]);
        }
        else {
            held_buttons.fetch_and(~(uint32_t)kN64Bits[key]);
        }
    }

    void on_tap(int button) {
        if (button == BUTTON_MENU) {
            menu_tapped = true;
        }
    }

    // descore's y grows downwards. Past half way the stick holds that way's C button, so a quick
    // double push rolls like a double tap of the C buttons does.
    void on_stick(int, float x, float y) {
        constexpr float threshold = 0.5f;
        uint32_t bits = 0;
        if (x < -threshold) bits |= kCLeft;
        if (x > threshold) bits |= kCRight;
        if (y < -threshold) bits |= kCUp;
        if (y > threshold) bits |= kCDown;
        held_move.store(bits);
    }

    void set_button(Button button, float x, float y, float w, float h) {
        const unsigned char key = (unsigned char)button;
        if (button == BUTTON_MENU) {
            descore_touch_set_button(button, x, y, w, h, nullptr, 0, false);
        }
        else {
            descore_touch_set_button(button, x, y, w, h, &key, 1, false);
        }
    }

    // Movement stick bottom left; FIRE bottom right with JUMP beside it and AIM above it; MENU and
    // START top centre (as in BattleTanx: lower down they cover the HUD). Sizes follow the screen
    // height and the Size setting. Everything else is the aiming area.
    void layout(int width, int height) {
        const float w = (float)width;
        const float h = (float)height;
        const float scale = descore_touch_scale_value(descore_touch_scale_step());
        const float c = std::min(std::max(h * 0.12f * scale, descore_touch_min_cell_px()), h * 0.18f);
        const float m = h * 0.04f;
        const float gap = c * 0.12f;

        descore_touch_set_screen_size(width, height);

        const float r = c * 1.25f;
        descore_touch_set_stick(0, m + r * 1.15f, h - m - r * 1.15f, r, on_stick);
        descore_touch_set_stick_style(0, DESCORE_TOUCH_STICK_ANALOG);

        const float fire = c * 1.3f;
        set_button(BUTTON_FIRE, w - m - fire, h - m - fire, fire, fire);
        set_button(BUTTON_JUMP, w - m - fire - gap - c, h - m - c, c, c);
        set_button(BUTTON_AIM, w - m - fire, h - m - fire - gap - c * 0.7f, fire, c * 0.7f);

        set_button(BUTTON_MENU, w * 0.5f - c * 1.4f - gap * 0.5f, m, c * 1.4f, c * 0.6f);
        set_button(BUTTON_START, w * 0.5f + gap * 0.5f, m, c * 1.4f, c * 0.6f);
    }

    bool buttons_hidden() {
        switch (setting_buttons.load()) {
            case snp::android::TouchButtons::Always: return false;
            case snp::android::TouchButtons::Never: return true;
            default: return gamepad_connected;
        }
    }

    void release_pointer(int id, float x, float y) {
        if (owners[id] == Owner::Buttons) {
            descore_touch_handle(DESCORE_TOUCH_ACTION_POINTER_UP, id, x, y);
        }
        else if (owners[id] == Owner::Aim) {
            aim_pointer = -1;
            aim_finger_fires.store(false);
        }
        owners[id] = Owner::None;
    }

    bool initialized = false;
}

void snp::android::set_touch_buttons(TouchButtons buttons) {
    setting_buttons.store(buttons);
    settings_changed.store(true);
}

void snp::android::set_touch_fire_while_touching(bool fire) {
    setting_fire_while_touching.store(fire);
}

void snp::android::set_touch_size(double percent) {
    setting_size.store(percent);
    settings_changed.store(true);
}

void snp::android::set_touch_opacity(double percent) {
    setting_opacity.store(percent);
    settings_changed.store(true);
}

void snp::android::add_touch_input(uint16_t* buttons) {
    *buttons |= (uint16_t)(held_buttons.load() | held_move.load());
    if (aim_finger_fires.load()) {
        *buttons |= kZ;
    }
}

extern "C" {

// Screen size; lays the buttons out again with the current settings.
JNIEXPORT void JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeLayout(
    JNIEnv*, jclass, jint width, jint height, jfloat density) {
    if (!initialized) {
        descore_touch_init(BUTTON_COUNT, on_key, on_tap);
        initialized = true;
    }
    settings_changed.store(false);
    view_w = width;
    view_h = height;
    descore_touch_set_display_density(density);
    descore_touch_set_scale_step(scale_step_for(setting_size.load()));
    descore_touch_set_opacity((float)(setting_opacity.load() / 100.0));
    layout(width, height);
    descore_touch_set_gamepad_connected(buttons_hidden());
}

// One pointer of a touch event (Android's action codes, which descore shares). Bit 0: taken (by
// a button or for aiming); bit 1: MENU was tapped; bit 2: a button or the stick had it, so the
// buttons need drawing again.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeTouch(
    JNIEnv*, jclass, jint action, jint pointer_id, jfloat x, jfloat y) {
    menu_tapped = false;
    if (!initialized || pointer_id < 0 || pointer_id >= DESCORE_TOUCH_MAX_POINTERS) {
        return 0;
    }
    bool taken = false;
    bool buttons = owners[pointer_id] == Owner::Buttons;
    switch (action) {
        case DESCORE_TOUCH_ACTION_DOWN:
        case DESCORE_TOUCH_ACTION_POINTER_DOWN:
            release_pointer(pointer_id, x, y);
            if (descore_touch_hit_test(x, y)) {
                owners[pointer_id] = Owner::Buttons;
                descore_touch_handle(action, pointer_id, x, y);
                taken = buttons = true;
            }
            else if (aim_pointer == -1 && snp::mouse::get_touch_aim() != snp::mouse::TouchAim::Off) {
                owners[pointer_id] = Owner::Aim;
                aim_pointer = pointer_id;
                snp::mouse::touch_aim_down(x, y, view_w, view_h);
                aim_finger_fires.store(setting_fire_while_touching.load());
                taken = true;
            }
            break;
        case DESCORE_TOUCH_ACTION_MOVE:
            if (owners[pointer_id] == Owner::Buttons) {
                descore_touch_handle(action, pointer_id, x, y);
                taken = true;
            }
            else if (owners[pointer_id] == Owner::Aim) {
                snp::mouse::touch_aim_move(x, y);
                taken = true;
            }
            break;
        case DESCORE_TOUCH_ACTION_UP:
        case DESCORE_TOUCH_ACTION_POINTER_UP:
            taken = owners[pointer_id] != Owner::None;
            release_pointer(pointer_id, x, y);
            break;
        default:
            break;
    }
    return (taken ? 1 : 0) | (menu_tapped ? 2 : 0) | (buttons ? 4 : 0);
}

// Lets go of every finger (the game's menu took the input, or the gesture was cancelled), so
// nothing stays held.
JNIEXPORT void JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeReleaseAll(
    JNIEnv*, jclass) {
    for (int id = 0; id < DESCORE_TOUCH_MAX_POINTERS; id++) {
        release_pointer(id, 0.0f, 0.0f);
    }
}

// Whether a gamepad is connected (a handheld's built-in one too): with Buttons on Auto they hide.
// Aiming stays.
JNIEXPORT void JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeSetGamepadConnected(
    JNIEnv*, jclass, jboolean connected) {
    gamepad_connected = connected;
    if (initialized) {
        descore_touch_set_gamepad_connected(buttons_hidden());
    }
}

// Shapes to draw, 7 floats each (kind, button, x, y, w, h, alpha); returns how many.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeGetShapes(
    JNIEnv* env, jclass, jfloatArray out) {
    if (!initialized) {
        return 0;
    }
    DescoreTouchShape shapes[DESCORE_TOUCH_MAX_BUTTONS + 16];
    const jsize capacity = env->GetArrayLength(out) / 7;
    const int count = descore_touch_get_shapes(shapes, std::min<int>(capacity, (int)(sizeof(shapes) / sizeof(shapes[0]))));
    jfloat* values = env->GetFloatArrayElements(out, nullptr);
    for (int i = 0; i < count; i++) {
        jfloat* v = values + i * 7;
        v[0] = (jfloat)shapes[i].kind;
        v[1] = (jfloat)shapes[i].button;
        v[2] = shapes[i].x;
        v[3] = shapes[i].y;
        v[4] = shapes[i].w;
        v[5] = shapes[i].h;
        v[6] = shapes[i].alpha;
    }
    env->ReleaseFloatArrayElements(out, values, 0);
    return count;
}

JNIEXPORT jstring JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativeButtonLabel(
    JNIEnv* env, jclass, jint button) {
    return env->NewStringUTF((button >= 0 && button < BUTTON_COUNT) ? kLabels[button] : "");
}

// Bit 0: touch is for the game now -- it's running and takes input (not on the launcher, nor
// while one of the game's menus has it). Bit 1: the settings changed, lay out again. Bit 2: the
// game has started.
JNIEXPORT jint JNICALL Java_io_github_wootbeer_snprecomp_TouchControlsView_nativePoll(
    JNIEnv*, jclass) {
    const bool started = ultramodern::is_game_started();
    const bool active = started && !recompinput::game_input_disabled();
    return (active ? 1 : 0) | (settings_changed.load() ? 2 : 0) | (started ? 4 : 0);
}

}
