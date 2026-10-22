// The Enhancements and Cheats tabs of the menu. The game reads them through
// src/game/extras.cpp.
#include "snp_config.h"

#include "recompui/config.h"

namespace {
    const std::string enhancements_id = "enhancements";
    const std::string cheats_id = "cheats";

    bool tabs_added = false;

    bool get_bool(const std::string& tab, const char* id) {
        if (!tabs_added) {
            return false;
        }
        return std::get<bool>(recompui::config::get_config(tab).get_option_value(id));
    }
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

    recomp::config::Config& cheats = recompui::config::create_config_tab("Cheats", cheats_id, false);
    cheats.add_bool_option("infinite_health", "Infinite Health", "", false);
    cheats.add_bool_option("infinite_time", "Infinite Time", "", false);
    cheats.add_bool_option("infinite_continues", "Infinite Continues", "", false);
}

bool snp::config::get_option(Option option) {
    switch (option) {
        case Option::UnlockOptions: return get_bool(enhancements_id, "unlock_options");
        case Option::InfiniteHealth: return get_bool(cheats_id, "infinite_health");
        case Option::InfiniteTime: return get_bool(cheats_id, "infinite_time");
        case Option::InfiniteContinues: return get_bool(cheats_id, "infinite_continues");
    }
    return false;
}
