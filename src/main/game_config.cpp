// Game-specific options on the General tab. See include/snp_config.h.
#include "snp_config.h"

#include "recompui/config.h"

namespace {
    const std::string rumble_pak_id = "rumble_pak";

    // Looked up by id on each read: the Config& create_general_tab() returns
    // lives in a container that moves when later tabs are created, so a
    // pointer kept from it dangles.
    bool options_added = false;
}

void snp::config::add_general_options(recomp::config::Config& general) {
    options_added = true;

    general.add_bool_option(
        rumble_pak_id,
        "Rumble Pak",
        "Plugs a Rumble Pak into the controller, so the game can vibrate it."
        "<br/><br/>"
        "Changing this is like swapping paks: the game notices the next time it checks.",
        true
    );
}

bool snp::config::get_rumble_pak() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(rumble_pak_id));
}
