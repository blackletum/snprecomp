#include "patches.h"
#include "ui_funcs.h"

// func_8004D748 is the game's frame swap: a job (func_8004D7C8's job thread
// runs it from a function pointer) that calls osViSwapBuffer with the job's
// framebuffer. It's the game's only per-frame osViSwapBuffer (libsched's
// __scTaskComplete is linked but the game runs its own scheduler), so it
// also runs recompui's per-frame UI callbacks (recomp_run_ui_callbacks).
// Same logic as the original (verified against the disassembly), with the
// framebuffer at offset 0x0C of the job.
RECOMP_PATCH void func_8004D748(u8* job) {
    recomp_run_ui_callbacks();
    osViSwapBuffer(*(void**)(job + 0x0C));
}
