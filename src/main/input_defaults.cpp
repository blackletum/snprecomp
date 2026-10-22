// The default controller layout: twin-stick. The game moves the character
// with the C buttons and aims with the stick, so the C buttons go on the left
// stick and the N64 stick on the right stick, with Z (fire) on the right
// trigger and R on the left; the C buttons' second bindings (face buttons,
// right stick click, right shoulder) and everything else stay the
// frontend's defaults. The keyboard keeps the frontend's keys, with the mouse
// buttons second: left click Z (fire), right click R (jump), middle click A
// (aim mode).
// These only apply to new profiles and "reset to defaults"; saved bindings
// (controls.json) win.
#include "recompinput/input_mapping.h"
#include "recompinput/input_types.h"
#include "recompinput/profiles.h"
#include "recompui/config.h"
#include "librecomp/game.hpp"

namespace snp {
    void set_default_controls() {
        using recompinput::GameInput;
        using recompinput::InputField;
        auto analog = [](SDL_GameControllerAxis axis, bool positive) { return InputField::controller_analog(axis, positive); };
        auto digital = [](SDL_GameControllerButton button) { return InputField::controller_digital(button); };

        recompinput::set_default_mapping_for_controller(GameInput::C_LEFT,  { analog(SDL_CONTROLLER_AXIS_LEFTX, false), digital(recompinput::SDL_CONTROLLER_BUTTON_NORTH) });
        recompinput::set_default_mapping_for_controller(GameInput::C_RIGHT, { analog(SDL_CONTROLLER_AXIS_LEFTX, true),  digital(recompinput::SDL_CONTROLLER_BUTTON_EAST) });
        recompinput::set_default_mapping_for_controller(GameInput::C_UP,    { analog(SDL_CONTROLLER_AXIS_LEFTY, false), digital(SDL_CONTROLLER_BUTTON_RIGHTSTICK) });
        recompinput::set_default_mapping_for_controller(GameInput::C_DOWN,  { analog(SDL_CONTROLLER_AXIS_LEFTY, true),  digital(SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) });

        recompinput::set_default_mapping_for_controller(GameInput::X_AXIS_NEG, { analog(SDL_CONTROLLER_AXIS_RIGHTX, false) });
        recompinput::set_default_mapping_for_controller(GameInput::X_AXIS_POS, { analog(SDL_CONTROLLER_AXIS_RIGHTX, true) });
        recompinput::set_default_mapping_for_controller(GameInput::Y_AXIS_NEG, { analog(SDL_CONTROLLER_AXIS_RIGHTY, true) });
        recompinput::set_default_mapping_for_controller(GameInput::Y_AXIS_POS, { analog(SDL_CONTROLLER_AXIS_RIGHTY, false) });

        recompinput::set_default_mapping_for_controller(GameInput::Z, { analog(SDL_CONTROLLER_AXIS_TRIGGERRIGHT, true) });
        recompinput::set_default_mapping_for_controller(GameInput::R, { analog(SDL_CONTROLLER_AXIS_TRIGGERLEFT, true) });

        auto key = [](SDL_Scancode scancode) { return InputField::keyboard(scancode); };
        auto mouse = [](uint8_t button) { return InputField{ recompinput::InputType::Mouse, button }; };
        recompinput::set_default_mapping_for_keyboard(GameInput::Z, { key(SDL_SCANCODE_Q), mouse(SDL_BUTTON_LEFT) });
        recompinput::set_default_mapping_for_keyboard(GameInput::R, { key(SDL_SCANCODE_R), mouse(SDL_BUTTON_RIGHT) });
        recompinput::set_default_mapping_for_keyboard(GameInput::A, { key(SDL_SCANCODE_SPACE), mouse(SDL_BUTTON_MIDDLE) });
    }

    // Once, for keyboard bindings saved before mouse buttons could be bound
    // (when they were fixed): the mouse buttons go in the free second slots.
    // Call after the config is loaded.
    void add_mouse_button_bindings() {
        const std::string done_id = "mouse_buttons_bound";
        recomp::config::Config& general = recompui::config::get_general_config();
        if (std::get<bool>(general.get_option_value(done_id))) {
            return;
        }
        using recompinput::GameInput;
        const std::pair<GameInput, uint8_t> buttons[] = {
            { GameInput::Z, SDL_BUTTON_LEFT }, { GameInput::R, SDL_BUTTON_RIGHT }, { GameInput::A, SDL_BUTTON_MIDDLE },
        };
        int profile = recompinput::profiles::get_sp_keyboard_profile_index();
        for (const auto& [input, button] : buttons) {
            if (recompinput::profiles::get_input_binding(profile, input, 1).input_type == recompinput::InputType::None) {
                recompinput::profiles::set_input_binding(profile, input, 1, { recompinput::InputType::Mouse, button });
            }
        }
        recompinput::profiles::save_controls_config(recomp::get_config_path() / (recompui::config::controls::id + ".json"));
        general.set_option_value(done_id, true);
        general.save_config();
    }
}
