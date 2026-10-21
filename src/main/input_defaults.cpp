// The default controller layout: twin-stick. The game moves the character
// with the C buttons and aims with the stick, so the C buttons go on the left
// stick and the N64 stick on the right stick, with Z (fire) on the right
// trigger and R on the left; the C buttons' second bindings (face buttons,
// right stick click, right shoulder) and everything else stay the
// frontend's defaults, as does the keyboard.
// These only apply to new profiles and "reset to defaults"; saved bindings
// (controls.json) win.
#include "recompinput/input_mapping.h"
#include "recompinput/input_types.h"

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
    }
}
