#ifndef __SNP_MOUSE_H__
#define __SNP_MOUSE_H__

#include <cstdint>

// Mouse aiming (src/main/mouse.cpp).
namespace snp::mouse {
    // Starts tracking the mouse. Call once SDL is up.
    void init();

    // Touch aiming (Android, from src/android/touch_controls.cpp): Point puts the
    // reticle under the finger, Drag moves it by the finger's movement, at
    // speed_percent (100: with the finger). Positions in the pixels of the touch
    // view, view_w x view_h, which covers the game's window.
    enum class TouchAim : uint32_t {
        Off = 0,
        Point = 1,
        Drag = 2,
    };
    void set_touch_aim(TouchAim aim);
    void set_touch_aim_speed(float speed_percent);
    TouchAim get_touch_aim();
    void touch_aim_down(float x, float y, int view_w, int view_h);
    void touch_aim_move(float x, float y);
}

#endif
