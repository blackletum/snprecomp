// The Enhancements and Cheats tabs of the menu, and the screen filter and
// bloom on the Graphics tab. The game reads them through src/game/extras.cpp.
#include "snp_config.h"

#include "recompui/config.h"

namespace {
    const std::string enhancements_id = "enhancements";
    const std::string cheats_id = "cheats";

    bool tabs_added = false;
    bool graphics_options_added = false;

    const std::string screen_filter_id = "screen_filter";
    const std::vector<recomp::config::ConfigOptionEnumOption> screen_filter_options = {
        {0, "None", "None"},
        {1, "CRT", "CRT"},
        {2, "Scanlines", "Scanlines"},
        {3, "LCD", "LCD"},
    };

    const std::string bloom_id = "bloom";
    const std::vector<recomp::config::ConfigOptionEnumOption> bloom_options = {
        {0, "Off", "Off"},
        {1, "Low", "Low"},
        {2, "High", "High"},
    };

    bool get_bool(const std::string& tab, const char* id) {
        if (!tabs_added) {
            return false;
        }
        return std::get<bool>(recompui::config::get_config(tab).get_option_value(id));
    }
}

// At the end of the Graphics tab.
void snp::config::add_graphics_options(recomp::config::Config& graphics) {
    graphics_options_added = true;
    graphics.add_enum_option(screen_filter_id, "Screen Filter", "", screen_filter_options, 0);
    graphics.add_enum_option(bloom_id, "Bloom", "", bloom_options, 0);
}

uint32_t snp::config::get_screen_filter() {
    if (!graphics_options_added) {
        return 0;
    }
    return std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(screen_filter_id));
}

uint32_t snp::config::get_bloom() {
    if (!graphics_options_added) {
        return 0;
    }
    return std::get<uint32_t>(recompui::config::get_graphics_config().get_option_value(bloom_id));
}

void snp::config::add_extras_tabs() {
    tabs_added = true;

    recomp::config::Config& enhancements = recompui::config::create_config_tab("Enhancements", enhancements_id, false);
    enhancements.add_bool_option(
        "unlock_options",
        "Unlock Everything",
        "Everything clearing the game on each difficulty unlocks: the main menu's OPT. menu with "
        "BGM and Alert Sound (Easy), Subtitles (Normal), Frame Skip and Game Speed (Hard), "
        "and every stage in Scene Select."
        "<br/><br/>"
        "Your save isn't changed: turning this off shows your own progress again.",
        false
    );
    enhancements.add_bool_option(
        "analog_aim",
        "Analog Aim",
        "Moves the reticle as far as the stick is pushed. The game takes the stick in steps, "
        "five speeds each way with a wide dead zone; with this on it takes every position, "
        "for finer aiming.",
        false
    );
    // On the Controls tab instead (lib-patches/RecompFrontend/0002).
    recompui::config::show_option_on_controls_tab(enhancements_id, "analog_aim");

    recomp::config::Config& cheats = recompui::config::create_config_tab("Cheats", cheats_id, false);
    cheats.add_bool_option("infinite_health", "Infinite Health", "", false);
    cheats.add_bool_option("infinite_time", "Infinite Time", "", false);
    cheats.add_bool_option("infinite_continues", "Infinite Continues", "", false);
}

bool snp::config::get_option(Option option) {
    switch (option) {
        case Option::UnlockOptions: return get_bool(enhancements_id, "unlock_options");
        case Option::AnalogAim: return get_bool(enhancements_id, "analog_aim");
        case Option::InfiniteHealth: return get_bool(cheats_id, "infinite_health");
        case Option::InfiniteTime: return get_bool(cheats_id, "infinite_time");
        case Option::InfiniteContinues: return get_bool(cheats_id, "infinite_continues");
    }
    return false;
}
