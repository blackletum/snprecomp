#ifndef __SNP_ANDROID_H__
#define __SNP_ANDROID_H__

#include <cstdint>

#if defined(__ANDROID__)
namespace snp::android {
    // Called first thing in main(): sends stdout/stderr to logcat (tag "SnP") and log.txt, lets
    // the renderer find the game's surface again after the app was in the background, and makes
    // the app's private storage folder the working directory, which is where the activity has
    // copied assets/ and where relative paths such as crash_log.txt land.
    void startup();

    // The on-screen touch controls (src/android/touch_controls.cpp): ORs in the N64 buttons they
    // hold (the movement stick as the C buttons, and Z while an aiming finger fires).
    void add_touch_input(uint16_t* buttons);

    // Their settings, from the Touch tab (src/main/game_config.cpp). Auto: shown unless
    // a gamepad is connected.
    enum class TouchButtons : uint32_t {
        Auto = 0,
        Always = 1,
        Never = 2,
    };
    void set_touch_buttons(TouchButtons buttons);
    void set_touch_fire_while_touching(bool fire);
    void set_touch_size(double percent);   // 90-180
    void set_touch_opacity(double percent); // 30-100
}
#endif

#endif
