#ifndef SNP_RECOMP_HOOKS_H
#define SNP_RECOMP_HOOKS_H

#include <stdint.h>

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in snp.jp.toml needs beyond what
// N64Recomp's generated recomp.h already provides.

#ifdef __cplusplus
extern "C" {
#endif

// src/game/rom_version.cpp: nonzero when the fan-translated English ROM is
// running.
int snp_rom_is_english(void);

// src/game/overlay_loader.cpp: registers a code segment the game loaded.
void snp_segment_loaded(uint8_t* rdram, int32_t args);

// src/game/spin_wait.cpp: lets other threads run from a busy-wait loop.
void snp_spin_yield(uint8_t* rdram);

#ifdef __cplusplus
}
#endif

#endif // SNP_RECOMP_HOOKS_H
