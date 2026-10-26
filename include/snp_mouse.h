#ifndef __SNP_MOUSE_H__
#define __SNP_MOUSE_H__

#include <cstdint>

// Mouse aiming (src/main/mouse.cpp).
namespace snp::mouse {
    // Starts tracking the mouse. Call once SDL is up.
    void init();
    // Smooth Aiming: the RT64 live offset ID for the reticle this frame, with
    // its position (x, y: offset from the screen centre, y up), or 0.
    uint32_t live_reticle_id(uint8_t* rdram, float& x, float& y);
}

#endif
