#ifndef __SNP_MOD_API_H__
#define __SNP_MOD_API_H__

// Functions mods can call (src/main/mod_api.cpp). In a mod:
//   RECOMP_IMPORT("*", u32 snp_get_player_count(void));
//   RECOMP_IMPORT("*", s32 snp_get_player_pad(s32 player, OSContPad* pad));
//   RECOMP_IMPORT("*", void snp_set_live_reticle(s32 slot, const SnpLiveReticle* reticle));
//   RECOMP_IMPORT("*", void snp_draw_overlay(const void* display_list));
//   RECOMP_IMPORT("*", const void* snp_get_scene_projection(void));
//   RECOMP_IMPORT("*", int recomp_printf(const char* fmt, ...));
#include <cstdint>

namespace snp {
    // Call before recomp::start(), so they're there when mods load.
    void register_mod_exports();
    // recomp_printf (src/main/mod_printf.cpp), from register_mod_exports.
    void register_printf_export();

    // Smooth Aiming for mods' crosshairs. The display list walker: the live
    // ID for a 2D matrix drawn at (x, y), if it's a mod's crosshair, else 0.
    uint32_t mod_live_reticle_id(float x, float y);
    // RT64's live offset callback (src/main/mouse.cpp), for a mod's slot.
    bool mod_live_offset(uint32_t slot, float game_x, float game_y, float* dx, float* dy);
    // The display list walker: the mod's overlay list for this frame
    // (snp_draw_overlay), or 0.
    uint32_t mod_overlay_list();
    // The display list walker: it copied the scene's projection to copy.
    void scene_projection_copied(uint32_t copy);
}

#endif
