#ifndef __SNP_CONFIG_H__
#define __SNP_CONFIG_H__

#include "librecomp/config.hpp"

// Game-specific options, shown on the General tab (src/main/game_config.cpp).
namespace snp::config {
    // Adds this game's options to the General tab. Call between
    // recompui::config::create_general_tab() and recompui::config::finalize().
    void add_general_options(recomp::config::Config& general);

    bool get_rumble_pak();

    // Mouse aiming (on the Controls tab; src/main/mouse.cpp).
    enum class MouseAim : uint32_t {
        Off = 0,
        Relative = 1, // the mouse moves the reticle, the cursor held in the window
        Absolute = 2, // the reticle sits where the pointer is
    };
    MouseAim get_mouse_aim();
    float get_mouse_sensitivity(); // percent, for Relative

    // Screen Filter and Bloom at the end of the Graphics tab (RT64's
    // ScreenFilter and Bloom values; src/main/extras_config.cpp).
    void add_graphics_options(recomp::config::Config& graphics);
    uint32_t get_screen_filter();
    uint32_t get_bloom();

    // The Enhancements and Cheats tabs (src/main/extras_config.cpp). Call
    // before recompui::config::finalize().
    void add_extras_tabs();

    enum class Option {
        UnlockOptions,
        AnalogAim,
        InfiniteHealth,
        InfiniteTime,
        InfiniteContinues,
    };
    bool get_option(Option option);
}

// Widescreen (src/game/widescreen.cpp): how much wider than 4:3 RT64 is
// drawing full-width 3D views -- the window aspect / (4/3) in Expand mode,
// 1 otherwise. Updated by the frontend every frame.
namespace snp {
    void set_widescreen_scale(float scale);
    float get_widescreen_scale();
}

#endif
