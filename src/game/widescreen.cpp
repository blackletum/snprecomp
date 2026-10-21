// Widescreen state shared with the game fixes. RT64's Expand mode draws the
// wider view itself; game-specific fixes (HUD anchoring, the crosshair's
// screen-to-world mapping, culling at the screen edges) read the scale from
// here as they're added.
#include <atomic>
#include <cstdint>
#include <cstring>

#include "snp_config.h"

namespace {
    std::atomic<float> widescreen_scale{ 1.0f };
}

void snp::set_widescreen_scale(float scale) {
    widescreen_scale.store(scale, std::memory_order_relaxed);
}

float snp::get_widescreen_scale() {
    return widescreen_scale.load(std::memory_order_relaxed);
}

// func_80029070's left/right visibility limit (snp.jp.toml): wider in Expand.
extern "C" float snp_cull_width_scale() {
    return snp::get_widescreen_scale();
}

// func_800AB924's left (side -1) and right (side 1) limits for the reticle's
// offset from the screen centre (snp.jp.toml), as float bits: 16 short of the
// screen's edge, 144 at 4:3.
extern "C" uint32_t snp_reticle_limit_bits(int32_t side) {
    float limit = side * (160.0f * snp::get_widescreen_scale() - 16.0f);
    uint32_t bits;
    memcpy(&bits, &limit, sizeof(bits));
    return bits;
}
