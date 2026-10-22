#ifndef __SNP_MOUSE_H__
#define __SNP_MOUSE_H__

#include <cstdint>

// Mouse aiming (src/main/mouse.cpp).
namespace snp::mouse {
    // Starts tracking the mouse. Call once SDL is up.
    void init();
}

#endif
