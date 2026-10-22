#ifndef __SNP_MOUSE_H__
#define __SNP_MOUSE_H__

#include <cstdint>

// Mouse aiming (src/main/mouse.cpp).
namespace snp::mouse {
    // Starts tracking the mouse. Call once SDL is up.
    void init();
    // ultramodern's get_input: the player's bindings plus the mouse buttons.
    bool get_input(int player, uint16_t* buttons, float* x, float* y);
}

#endif
