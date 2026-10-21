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
// the border is dropped. Fades and tints, a full-screen quad drawn with the
// 2D projection, are widened (widen_screen_quad), and frames that start with
// a full-screen 4:3 picture get a clear to black first (clear_prefix).
// Original aspect is left alone.
//
// Diagnostic: with SNP_DL_DIAG set, every SNP_DL_DIAG_INTERVAL-th (default
// 300) graphics task's color images, scissors, viewports, matrices, small
// vertex loads, fill and texture rectangles, textures, colors, combine and
// render modes are printed as they're walked.
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
    constexpr uint32_t G_NOOP = 0x00, G_VTX = 0x01, G_FILLRECT = 0xF6, G_SETSCISSOR = 0xED;

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

    // The frame's first color image command, and whether its first drawing
    // command is a texture rectangle at the screen's top left corner.
    uint32_t first_cimg_w0, first_cimg_w1;
    bool seen_draw;
    bool starts_with_picture;

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

    // Copies of full-screen quads, widened (widen_screen_quad), in RAM the game
    // doesn't use: it lays itself out in the first 4 MB (the last framebuffer
    // ends at 0x80400000). Used in turn, so a frame the renderer hasn't read
    // yet keeps its copies.
    constexpr uint32_t quad_area = 0x807F0000;
    constexpr uint32_t quad_slots = 64;
    constexpr uint32_t quad_bytes = 4 * 16;
    uint32_t quad_slot = 0;

    // Fades and tints (scene cross-fades, the red flash, the eyes shot's black)
    // are a 4-vertex quad over the whole screen, -160..160
    // by -120..120, drawn with the game's 2D projection. RT64 keeps 2D
    // projections at 4:3 so the HUD isn't stretched, which left these as a 4:3
    // box. In Expand, the vertex load is pointed at a copy (made every frame,
    // after the game has set this frame's colors) four times as wide, clipped to
    // the screen by the scissor. The game's vertices are left alone; the command
    // is in the frame's own display list, rebuilt each frame.
    void widen_screen_quad(uint8_t* rdram, uint32_t addr, uint32_t w0, uint32_t w1) {
        if (((w0 >> 12) & 0xFF) != 4) {
            return;
        }
        uint32_t src = resolve(w1);
        for (uint32_t i = 0; i < 4; i++) {
            uint32_t xy_word = (uint32_t)MEM_W(i * 16, (gpr)(int32_t)src);
            int16_t x = (int16_t)(xy_word >> 16), y = (int16_t)xy_word;
            if ((x != 160 && x != -160) || (y != 120 && y != -120)) {
                return;
            }
        }
        uint32_t dst = quad_area + quad_slot * quad_bytes;
        quad_slot = (quad_slot + 1) % quad_slots;
        for (uint32_t off = 0; off < quad_bytes; off += 4) {
            uint32_t word = (uint32_t)MEM_W(off, (gpr)(int32_t)src);
            if (off % 16 == 0) {
                int16_t x = (int16_t)(word >> 16);
                word = ((uint32_t)(uint16_t)(x * 4) << 16) | (word & 0xFFFF);
            }
            MEM_W(off, (gpr)(int32_t)dst) = (int32_t)word;
        }
        set_command(rdram, addr, w0, dst);
    }

    // Frames that start with a full-screen picture (the title screen's backdrop,
    // drawn as tiles) don't clear the screen: on the N64 the picture covers it.
    // In Expand the picture stays 4:3 (stretching it would distort the art), so
    // the sides kept whatever was drawn there before. Such frames get a prefix
    // that clears the whole screen to black first and then branches to the
    // game's display list. Used in turn, like the quad copies.
    constexpr uint32_t prefix_area = 0x807F2000;
    constexpr uint32_t prefix_slots = 8;
    constexpr uint32_t prefix_bytes = 8 * 8;
    uint32_t prefix_slot = 0;

    uint32_t clear_prefix(uint8_t* rdram, uint32_t dl) {
        uint32_t prefix = prefix_area + prefix_slot * prefix_bytes;
        prefix_slot = (prefix_slot + 1) % prefix_slots;
        const uint32_t commands[][2] = {
            { 0xE3000A01, 0x00300000 }, // cycle type: fill
            { first_cimg_w0, first_cimg_w1 },
            { full_scissor_w0, full_scissor_w1 },
            { 0xF7000000, 0x00010001 }, // fill color: black, as the game's own clears
            { (G_FILLRECT << 24) | xy(319, 239), xy(0, 0) },
            { 0xDE010000, dl }, // branch to the game's display list
        };
        uint32_t addr = prefix;
        for (const auto& c : commands) {
            set_command(rdram, addr, c[0], c[1]);
            addr += 8;
        }
        return prefix;
    }

    bool is_prefix(uint32_t addr) {
        return addr >= prefix_area && addr < prefix_area + prefix_slots * prefix_bytes;
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
            case 0xE3: printf("[DL] %*sothermode_H %08X %08X\n", depth, "", w0, w1); break;
            case 0xFA: printf("[DL] %*sprim color %08X\n", depth, "", w1); break;
            case 0xFB: printf("[DL] %*senv color %08X\n", depth, "", w1); break;
            case 0x01: {
                printf("[DL] %*svertices %08X %08X\n", depth, "", w0, w1);
                uint32_t count = (w0 >> 12) & 0xFF;
                if (count <= 4) {
                    uint32_t v = resolve(w1);
                    for (uint32_t i = 0; i < count; i++, v += 16) {
                        uint32_t a = (uint32_t)MEM_W(0, (gpr)(int32_t)v), b = (uint32_t)MEM_W(4, (gpr)(int32_t)v), c = (uint32_t)MEM_W(12, (gpr)(int32_t)v);
                        printf("[DL] %*s  v %d %d %d rgba %08X\n", depth, "", (int16_t)(a >> 16), (int16_t)a, (int16_t)(b >> 16), c);
                    }
                }
                break;
            }
            case 0x05: case 0x06: printf("[DL] %*striangles %08X %08X\n", depth, "", w0, w1); break;
            case 0xDA: {
                printf("[DL] %*smatrix %08X %08X\n", depth, "", w0, w1);
                uint32_t m = resolve(w1);
                printf("[DL] %*s  m", depth, "");
                for (int i = 0; i < 16; i++) {
                    int32_t hi = (int16_t)((uint32_t)MEM_W((i / 2) * 4, (gpr)(int32_t)m) >> ((i & 1) ? 0 : 16));
                    uint32_t lo = (uint16_t)((uint32_t)MEM_W(32 + (i / 2) * 4, (gpr)(int32_t)m) >> ((i & 1) ? 0 : 16));
                    printf(" %.4f", (int32_t)(((uint32_t)hi << 16) | lo) / 65536.0);
                }
                printf("\n");
                break;
            }
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
                else if ((w0 >> 24) == G_VTX) {
                    widen_screen_quad(rdram, addr, w0, w1);
                }
                else {
                    for (const auto& b : border) {
                        if (w0 == b[0] && w1 == b[1]) {
                            set_command(rdram, addr, w0 = G_NOOP << 24, w1 = 0);
                        }
                    }
                }
            }
            switch (w0 >> 24) {
                case 0xFF: // G_SETCIMG
                    if (first_cimg_w0 == 0) {
                        first_cimg_w0 = w0;
                        first_cimg_w1 = w1;
                    }
                    break;
                case 0xE4: // G_TEXRECT
                    if (!seen_draw) {
                        starts_with_picture = (w1 & 0x00FFFFFF) == 0;
                    }
                    seen_draw = true;
                    break;
                case 0xF6: // G_FILLRECT
                case 0x05: case 0x06: case 0x07: // G_TRI1, G_TRI2, G_QUAD
                    seen_draw = true;
                    break;
                default:
                    break;
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

    uint32_t dl = (uint32_t)MEM_W(0x30, task); // OSTask.data_ptr
    if (is_prefix(dl)) { // a task loaded again: the game's list is the branch target
        dl = (uint32_t)MEM_W(5 * 8 + 4, (gpr)(int32_t)dl);
        MEM_W(0x30, task) = (int32_t)dl;
    }
    bool widen = snp::get_widescreen_scale() > 1.0f;
    if (!widen && !log_frame) {
        return;
    }
    if (log_frame) {
        static const auto start = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        printf("[DL] ---- frame %d at %.1f s, display list %08X%s\n", frame - 1, t, dl, widen ? " (widened)" : "");
    }
    for (uint32_t& s : segments) {
        s = 0;
    }
    first_cimg_w0 = 0;
    seen_draw = false;
    starts_with_picture = false;
    walk(rdram, dl, 0, widen);
    if (widen && starts_with_picture && first_cimg_w0 != 0) {
        MEM_W(0x30, task) = (int32_t)clear_prefix(rdram, dl);
    }
    if (log_frame) {
        fflush(stdout);
    }
}
