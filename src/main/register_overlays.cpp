// Registers N64Recomp's generated section table
// (RecompiledFuncs/recomp_overlays.inl) with librecomp, which indirect calls
// (function pointers, jump tables) look their targets up in.
#ifdef SNP_HAS_RECOMPILED_FUNCS

#include "librecomp/overlays.hpp"

#include "../../RecompiledFuncs/recomp_overlays.inl"

void snp_install_crash_handler(const SectionTableEntry* sections, size_t num_sections);

void register_snp_overlays() {
    recomp::overlays::overlay_section_table_data_t sections{
        .code_sections = section_table,
        .num_code_sections = ARRLEN(section_table),
        .total_num_sections = num_sections,
    };

    recomp::overlays::overlays_by_index_t overlays{
        .table = overlay_sections_by_index,
        .len = ARRLEN(overlay_sections_by_index),
    };

    recomp::overlays::register_overlays(sections, overlays);

    // Crash reports name the faulting game function (src/main/crash_handler.cpp).
    snp_install_crash_handler(section_table, ARRLEN(section_table));
}

#else

// No RecompiledFuncs/ yet (N64Recomp not run, see BUILDING.md).
void register_snp_overlays() {}

#endif
