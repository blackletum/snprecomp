#ifndef SNP_RECOMP_HOOKS_H
#define SNP_RECOMP_HOOKS_H

// Force-included into every file in RecompiledFuncs/ (see CMakeLists.txt),
// for whatever the [[patches.hook]] text in snp.jp.toml needs beyond what
// N64Recomp's generated recomp.h already provides.

#ifdef __cplusplus
extern "C" {
#endif

// src/game/rom_version.cpp: nonzero when the fan-translated English ROM is
// running.
int snp_rom_is_english(void);

#ifdef __cplusplus
}
#endif

#endif // SNP_RECOMP_HOOKS_H
