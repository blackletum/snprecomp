// Game-specific options on the General tab. See include/snp_config.h.
#include "snp_config.h"

#include "recompui/config.h"

namespace {
    const std::string rumble_pak_id = "rumble_pak";
    const std::string mouse_aim_id = "mouse_aim";
    const std::string mouse_sensitivity_id = "mouse_sensitivity_relative";

    const std::vector<recomp::config::ConfigOptionEnumOption> mouse_aim_options = {
        {0, "Off", "Off"},
        {1, "Relative", "Relative"},
        {2, "Absolute", "Absolute"},
    };

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

    general.add_enum_option(
        mouse_aim_id,
        "Mouse Aim",
        "How the mouse moves the reticle. Left click fires (Z), right click jumps (R), "
        "middle click switches the aim mode (A)."
        "<br/><br/>"
        "<b>Absolute</b>: the reticle is where the mouse pointer is, like a light gun."
        "<br/>"
        "<b>Relative</b>: the mouse moves the reticle by Mouse Sensitivity, and the "
        "pointer stays in the window."
        "<br/><br/>"
        "A controller works as before either way.",
        mouse_aim_options,
        2
    );
    general.add_percent_number_option(
        mouse_sensitivity_id,
        "Mouse Sensitivity",
        "How far the reticle moves for the mouse with Mouse Aim on Relative. "
        "At 50% it moves with the pointer.",
        50.0
    );

    // Shown on the Controls tab, above the mappings, instead
    // (lib-patches/RecompFrontend/0002).
    recompui::config::show_option_on_controls_tab(recompui::config::general::id, mouse_aim_id);
    recompui::config::show_option_on_controls_tab(recompui::config::general::id, mouse_sensitivity_id);
    recompui::config::show_option_on_controls_tab(recompui::config::general::id, recompui::config::general::options::joystick_deadzone);
}

snp::config::MouseAim snp::config::get_mouse_aim() {
    if (!options_added) {
        return MouseAim::Off;
    }
    return (MouseAim)std::get<uint32_t>(recompui::config::get_general_config().get_option_value(mouse_aim_id));
}

float snp::config::get_mouse_sensitivity() {
    if (!options_added) {
        return 50.0f;
    }
    return (float)std::get<double>(recompui::config::get_general_config().get_option_value(mouse_sensitivity_id));
}

bool snp::config::get_rumble_pak() {
    if (!options_added) {
        return false;
    }
    return std::get<bool>(recompui::config::get_general_config().get_option_value(rumble_pak_id));
}
