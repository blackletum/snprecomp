#include "patches.h"
#include "ui_funcs.h"

// D_80060BBC: libsched's "unblank the display on the next swap" flag.
extern s32 D_80060BBC;

// func_80046B20 is libsched's __scTaskComplete(OSSched* sc, OSScTask* t): when
// a task's RSP and RDP work is done, it posts the task's done message and,
// for the last graphics task of a frame, swaps to its framebuffer. It's the
// game's only per-frame osViSwapBuffer, so it also runs recompui's per-frame
// UI callbacks (recomp_run_ui_callbacks). Same logic as the original
// (verified against the disassembly), with OSScTask fields as raw offsets:
//   0x04 state, 0x08 flags, 0x0C framebuffer, 0x10 list.t.type,
//   0x50 msgQ, 0x54 msg.
RECOMP_PATCH s32 func_80046B20(void* sc, u8* t) {
    if ((*(u32*)(t + 0x04) & 0x3) != 0) {
        return 0;
    }

    osSendMesg(*(OSMesgQueue**)(t + 0x50), *(OSMesg*)(t + 0x54), OS_MESG_BLOCK);

    if (*(u32*)(t + 0x10) == M_GFXTASK && (*(u32*)(t + 0x08) & 0x60) == 0x60) {
        recomp_run_ui_callbacks();
        if (D_80060BBC != 0) {
            osViBlack(FALSE);
            D_80060BBC = 0;
        }
        osViSwapBuffer(*(void**)(t + 0x0C));
    }
    return 1;
}
