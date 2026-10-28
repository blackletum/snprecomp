// Functions mods can call (include/snp_mod_api.h). A mod declares them with
// RECOMP_IMPORT("*", ...), as it does the runtime's own (recomp_alloc).
//
// snp_get_player_count: how many players have their own input. 1 unless
// Local Multiplayer (Mods) is on, then the number assigned on the Controls tab.
//
// snp_get_player_pad: a player's controller this moment, filled in like the
// game's own OSContPad { u16 button; s8 stick_x; s8 stick_y; u8 errno; },
// with the stick in the same N64 range the game gets. Returns 1, or 0 with an
// empty pad when that player has no input. Player 0 is player 1, without
// Android's touch controls. While one of the frontend's menus has the input
// the pad is empty, as it is for the game.
//
// With Local Multiplayer on, the game still sees port 1 only
// (src/main/main.cpp), so a mod is the only thing reading the other players.
#include <cstdint>

#include "librecomp/helpers.hpp"
#include "librecomp/overlays.hpp"
#include "recompinput/players.h"
#include "recompinput/profiles.h"

#include "snp_mod_api.h"

// ultramodern/src/input.cpp, not declared in a header: the stick conversion
// osContGetReadData uses.
void convert_to_n64_range(float x, float y, int8_t& stick_x, int8_t& stick_y);

namespace {
    uint32_t player_count() {
        if (recompinput::players::is_single_player_mode()) {
            return 1;
        }
        size_t assigned = recompinput::players::get_number_of_assigned_players();
        return (uint32_t)(assigned < 1 ? 1 : assigned);
    }
}

extern "C" void snp_get_player_count(uint8_t* rdram, recomp_context* ctx) {
    (void)rdram;
    _return(ctx, player_count());
}

extern "C" void snp_get_player_pad(uint8_t* rdram, recomp_context* ctx) {
    int32_t player = _arg<0, int32_t>(rdram, ctx);
    PTR(void) pad = _arg<1, PTR(void)>(rdram, ctx);

    uint16_t buttons = 0;
    int8_t stick_x = 0, stick_y = 0;
    bool has_input = player >= 0 && (uint32_t)player < player_count() &&
        (player == 0 || recompinput::players::get_player_is_assigned(player));
    if (has_input) {
        float x = 0.0f, y = 0.0f;
        recompinput::profiles::get_n64_input(player, &buttons, &x, &y);
        convert_to_n64_range(x, y, stick_x, stick_y);
    }

    if (pad != NULLPTR) {
        gpr addr = (gpr)pad;
        MEM_H(0, addr) = buttons;
        MEM_B(2, addr) = stick_x;
        MEM_B(3, addr) = stick_y;
        MEM_B(4, addr) = has_input ? 0 : 8; // CONT_NO_RESPONSE_ERROR, as osContGetReadData
    }
    _return(ctx, has_input ? 1 : 0);
}

void snp::register_mod_exports() {
    recomp::overlays::register_base_export("snp_get_player_count", snp_get_player_count);
    recomp::overlays::register_base_export("snp_get_player_pad", snp_get_player_pad);
}
