#ifndef __SNP_MOD_API_H__
#define __SNP_MOD_API_H__

// Functions mods can call (src/main/mod_api.cpp). In a mod:
//   RECOMP_IMPORT("*", u32 snp_get_player_count(void));
//   RECOMP_IMPORT("*", s32 snp_get_player_pad(s32 player, OSContPad* pad));
namespace snp {
    // Call before recomp::start(), so they're there when mods load.
    void register_mod_exports();
}

#endif
