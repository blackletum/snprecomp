// Widescreen state shared with the game fixes. RT64's Expand mode draws the
// wider view itself; game-specific fixes (HUD anchoring, the crosshair's
// screen-to-world mapping, culling at the screen edges) read the scale from
// here as they're added.
#include <atomic>

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
