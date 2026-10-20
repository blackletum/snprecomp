// The game's frame display lists, just before a task thread loads them
// (hooked before each osSpTaskLoad in func_8004E640 / func_8004E4A0,
// snp.jp.toml).
//
// Widescreen (STATUS.md round 5). The game draws every frame inside an
// inset, scissor 12,8 - 308,231, clears only that area, and at the end draws
// a black border around it with the scissor opened to the full screen. RT64
// widens a 3D view in Expand only when its scissor spans the whole screen
// width, so the picture stayed 4:3, and the side areas, which nothing clears,
// kept whatever was drawn there before (old frames, and the crosshair's
// translucent full-width lines piling up into stripes). In Expand, the inset
// scissor becomes the full screen, fill rectangles spanning the inset's
// width (the clear, fades and tints, backdrop bands) span the screen, and
// the border is dropped. Original aspect is left alone.
//
// Diagnostic: with SNP_DL_DIAG set, every SNP_DL_DIAG_INTERVAL-th (default
// 300) graphics task's color images, scissors, viewports, fill and texture
// rectangles, combine and render modes are printed as they're walked.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstdint>

#include "recomp.h"

#include "snp_config.h"
#include "snp_recomp_hooks.h"

namespace {
    // G_SETSCISSOR / G_FILLRECT words: coordinates are 10.2 fixed point.
    constexpr uint32_t xy(uint32_t x, uint32_t y) {
        return ((x * 4) << 12) | (y * 4);
    }
    constexpr uint32_t G_NOOP = 0x00, G_FILLRECT = 0xF6, G_SETSCISSOR = 0xED;

    // The inset scissor and its clear, and what they become in Expand.
    constexpr uint32_t inset_scissor_w0 = (G_SETSCISSOR << 24) | xy(12, 8);
    constexpr uint32_t inset_scissor_w1 = xy(308, 231);
    constexpr uint32_t full_scissor_w0 = (G_SETSCISSOR << 24) | xy(0, 0);
    constexpr uint32_t full_scissor_w1 = xy(320, 240);

    // The border's four fill rectangles: {w0, w1}.
    constexpr uint32_t border[4][2] = {
        { (G_FILLRECT << 24) | xy(308, 9), xy(12, 7) },
        { (G_FILLRECT << 24) | xy(308, 232), xy(12, 230) },
        { (G_FILLRECT << 24) | xy(13, 231), xy(11, 8) },
        { (G_FILLRECT << 24) | xy(309, 231), xy(307, 8) },
    };

    uint32_t segments[16];
    bool log_frame = false;

    void set_command(uint8_t* rdram, uint32_t addr, uint32_t w0, uint32_t w1) {
        MEM_W(0, (gpr)(int32_t)addr) = (int32_t)w0;
        MEM_W(4, (gpr)(int32_t)addr) = (int32_t)w1;
    }

    uint32_t resolve(uint32_t addr) {
        if (addr & 0x80000000u) {
            return addr;
        }
        return 0x80000000u | ((segments[(addr >> 24) & 0xF] + (addr & 0x00FFFFFF)) & 0x00FFFFFF);
    }

    void log_command(int depth, uint8_t* rdram, uint32_t w0, uint32_t w1) {
        auto rect = [&](const char* what, uint32_t ul, uint32_t lr) {
            printf("[DL] %*s%s %.2f,%.2f - %.2f,%.2f\n", depth, "", what,
                ((ul >> 12) & 0xFFF) / 4.0, (ul & 0xFFF) / 4.0, ((lr >> 12) & 0xFFF) / 4.0, (lr & 0xFFF) / 4.0);
        };
        switch (w0 >> 24) {
            case 0xDC:
                if ((w0 & 0xFF) == 8) { // G_MV_VIEWPORT
                    uint32_t vp = resolve(w1);
                    int32_t scale = MEM_W(0, (gpr)(int32_t)vp), trans = MEM_W(8, (gpr)(int32_t)vp);
                    printf("[DL] %*sviewport scale %.2f,%.2f trans %.2f,%.2f\n", depth, "",
                        (int16_t)(scale >> 16) / 4.0, (int16_t)scale / 4.0, (int16_t)(trans >> 16) / 4.0, (int16_t)trans / 4.0);
                }
                break;
            case 0xFF: printf("[DL] %*scolor image %08X width %u\n", depth, "", w1, (w0 & 0xFFF) + 1); break;
            case 0xED: rect("scissor", w0, w1); break;
            case 0xF7: printf("[DL] %*sfill color %08X\n", depth, "", w1); break;
            case 0xF6: rect("fill rect", w1, w0); break;
            case 0xE4: rect("tex rect", w1, w0); break;
            case 0xFC: printf("[DL] %*scombine %08X %08X\n", depth, "", w0, w1); break;
            case 0xDE: printf("[DL] %*sdisplay list %08X %08X\n", depth, "", w0, w1); break;
            case 0xFD: printf("[DL] %*stexture image %08X fmt %u siz %u width %u\n", depth, "", w1, (w0 >> 21) & 7, (w0 >> 19) & 3, (w0 & 0xFFF) + 1); break;
            case 0xF5: printf("[DL] %*ssettile %08X %08X\n", depth, "", w0, w1); break;
            case 0xF4: printf("[DL] %*sloadtile %08X %08X\n", depth, "", w0, w1); break;
            case 0x64: printf("[DL] %*sextended %08X %08X\n", depth, "", w0, w1); break;
            case 0xE2: printf("[DL] %*sothermode_L %08X %08X\n", depth, "", w0, w1); break;
            default: break;
        }
    }

    void walk(uint8_t* rdram, uint32_t addr, int depth, bool widen) {
        for (int n = 0; n < 200000; n++, addr += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
            uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)addr);
            if (widen) {
                if (w0 == inset_scissor_w0 && w1 == inset_scissor_w1) {
                    set_command(rdram, addr, w0 = full_scissor_w0, w1 = full_scissor_w1);
                }
                else if ((w0 >> 24) == G_FILLRECT && ((w0 >> 12) & 0xFFF) == 307 * 4 && ((w1 >> 12) & 0xFFF) == 12 * 4) {
                    // A fill rectangle across the inset's full width (the clear, fades
                    // and tints, a backdrop band): full screen width.
                    w0 = (w0 & 0xFF000FFF) | ((319 * 4) << 12);
                    w1 = (w1 & 0xFF000FFF) | (0 << 12);
                    // Covering the inset's full height too: the full screen height,
                    // or the rows above and below the inset, only ever covered by
                    // the border, keep old frames.
                    if ((w0 & 0xFFF) == 231 * 4 && (w1 & 0xFFF) == 8 * 4) {
                        w0 = (w0 & 0xFFFFF000) | (239 * 4);
                        w1 = (w1 & 0xFFFFF000) | 0;
                    }
                    set_command(rdram, addr, w0, w1);
                }
                else {
                    for (const auto& b : border) {
                        if (w0 == b[0] && w1 == b[1]) {
                            set_command(rdram, addr, w0 = G_NOOP << 24, w1 = 0);
                        }
                    }
                }
            }
            if (log_frame) {
                log_command(depth, rdram, w0, w1);
            }
            switch (w0 >> 24) {
                case 0xDE: // G_DL
                    if (depth < 10) {
                        walk(rdram, resolve(w1), depth + 1, widen);
                    }
                    if ((w0 >> 16) & 0xFF) { // branch: no return
                        return;
                    }
                    break;
                case 0xDF: // G_ENDDL
                    return;
                case 0xDB: // G_MOVEWORD
                    if (((w0 >> 16) & 0xFF) == 6) { // G_MW_SEGMENT
                        segments[((w0 & 0xFFFF) / 4) & 0xF] = w1 & 0x00FFFFFF;
                    }
                    break;
                case 0xE4: // G_TEXRECT: two more words follow (RDPHALF_1, RDPHALF_2)
                    addr += 16;
                    break;
                default:
                    break;
            }
        }
    }
}

// task: the OSTask about to be loaded.
extern "C" void snp_display_list_hook(uint8_t* rdram, int32_t task) {
    if (MEM_W(0x0, task) != 1) { // only graphics tasks (M_GFXTASK)
        return;
    }
    static const char* diag = std::getenv("SNP_DL_DIAG");
    static int interval = [] { const char* v = std::getenv("SNP_DL_DIAG_INTERVAL"); return v ? std::atoi(v) : 300; }();
    static int frame = 0;
    log_frame = diag != nullptr && (frame++ % interval) == 0;

    bool widen = snp::get_widescreen_scale() > 1.0f;
    if (!widen && !log_frame) {
        return;
    }
    uint32_t dl = (uint32_t)MEM_W(0x30, task); // OSTask.data_ptr
    if (log_frame) {
        static const auto start = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        printf("[DL] ---- frame %d at %.1f s, display list %08X%s\n", frame - 1, t, dl, widen ? " (widened)" : "");
    }
    for (uint32_t& s : segments) {
        s = 0;
    }
    walk(rdram, dl, 0, widen);
    if (log_frame) {
        fflush(stdout);
    }
}
