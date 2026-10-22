// Edits to each frame's display list just before a task thread loads it
// (hooked before each osSpTaskLoad, snp.jp.toml): widescreen, HUD anchoring
// and interpolation IDs.
//
// Widescreen: the game draws every frame inside an inset (scissor 12,8 -
// 308,231), clears only that area and draws a black border around it. RT64
// only widens a 3D view whose scissor spans the screen, so in Expand the inset
// scissor and the fill rectangles spanning it become full screen and the
// border is dropped. Original aspect is left alone.
//
// Diagnostic: with SNP_DL_DIAG set, every SNP_DL_DIAG_INTERVAL-th (default
// 300) graphics task's commands of interest are printed as they're walked.
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "recomp.h"

#include "snp_config.h"
#include "snp_recomp_hooks.h"

namespace snp {
    void interp_new_frame(); // interpolation.cpp
    uint32_t interp_matrix_id(uint32_t addr, uint32_t& instance, bool& object);
    void interp_number_instances(const std::vector<uint32_t>& drawn);
    uint32_t interp_numbered_id(uint32_t id, uint32_t instance);
}

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
    bool effect_area_scissor; // the game's own 0,8 - 320,231 scissor was set

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

    // Scratch lists and copies go in RAM the game doesn't use (it only uses
    // the first 4 MB). Each area is used in turn, so a frame the renderer
    // hasn't read yet keeps its copies.
    constexpr uint32_t quad_area = 0x807F0000;
    constexpr uint32_t quad_slots = 64;
    constexpr uint32_t quad_bytes = 4 * 16;
    uint32_t quad_slot = 0;

    // Fades and tints are a 4-vertex quad over the screen (-160..160 by
    // -120..120) drawn with the 2D projection, which RT64 keeps at 4:3. In
    // Expand the vertex load points at a copy four times as wide.
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

    // A prefix that clears the screen to black and then branches to the
    // game's list: for frames that start with a full-screen 4:3 picture (the
    // title backdrop; nothing clears the sides in Expand), and after switching
    // back to Original.
    constexpr uint32_t prefix_area = 0x807F2000;
    constexpr uint32_t prefix_slots = 8;
    constexpr uint32_t prefix_commands = 16; // the branch to the game's list is the last
    constexpr uint32_t prefix_bytes = prefix_commands * 8;

    // The screen-wide color images a frame draws to, in order (in gameplay
    // the first is an effect buffer, not the framebuffer shown).
    constexpr uint32_t max_cimgs = 4;
    uint32_t cimgs[max_cimgs][2];
    uint32_t cimg_count = 0;

    uint32_t prefix_slot = 0;

    // all: clear every color image the frame draws to, not just the first.
    uint32_t clear_prefix(uint8_t* rdram, uint32_t dl, bool all) {
        uint32_t prefix = prefix_area + prefix_slot * prefix_bytes;
        prefix_slot = (prefix_slot + 1) % prefix_slots;
        uint32_t commands[prefix_commands][2];
        uint32_t n = 0;
        auto cmd = [&](uint32_t a, uint32_t b) { commands[n][0] = a; commands[n][1] = b; n++; };
        cmd(0xE3000A01, 0x00300000); // cycle type: fill
        cmd(0xF7000000, 0x00010001); // fill color: black, as the game's own clears
        for (uint32_t i = 0; i < (all ? cimg_count : 1); i++) {
            cmd(cimgs[i][0], cimgs[i][1]);
            cmd(full_scissor_w0, full_scissor_w1);
            cmd((G_FILLRECT << 24) | xy(319, 239), xy(0, 0));
        }
        if (all && cimg_count > 1) {
            cmd(cimgs[0][0], cimgs[0][1]); // the game's list starts on the first
        }
        while (n < prefix_commands - 1) {
            cmd(G_NOOP << 24, 0);
        }
        cmd(0xDE010000, dl); // branch to the game's display list
        for (uint32_t i = 0; i < n; i++) {
            set_command(rdram, prefix + i * 8, commands[i][0], commands[i][1]);
        }
        return prefix;
    }

    // The zoom into the title draws its effect over rows 8 - 231 only; Expand
    // shows all 240 rows, so the rest kept the title picture. Such frames end
    // with rows 0 - 7 and 232 - 239 filled black: the closing full sync is
    // replaced with a branch to a list that fills them, syncs and ends.
    constexpr uint32_t suffix_area = 0x807F3000;
    constexpr uint32_t suffix_slots = 8;
    constexpr uint32_t suffix_bytes = 8 * 8;
    uint32_t suffix_slot = 0;
    constexpr uint32_t effect_scissor_w0 = (G_SETSCISSOR << 24) | xy(0, 8);
    constexpr uint32_t effect_scissor_w1 = xy(320, 231);

    void black_edge_rows(uint8_t* rdram, uint32_t fullsync_addr) {
        uint32_t suffix = suffix_area + suffix_slot * suffix_bytes;
        suffix_slot = (suffix_slot + 1) % suffix_slots;
        const uint32_t commands[][2] = {
            { 0xE3000A01, 0x00300000 }, // cycle type: fill
            { first_cimg_w0, first_cimg_w1 }, // the screen (the frame may end drawing elsewhere)
            { full_scissor_w0, full_scissor_w1 },
            { 0xF7000000, 0x00010001 }, // fill color: black
            { (G_FILLRECT << 24) | xy(319, 7), xy(0, 0) },
            { (G_FILLRECT << 24) | xy(319, 239), xy(0, 232) },
            { 0xE9000000, 0 }, // the game's full sync, moved after the fills
            { 0xDF000000, 0 },
        };
        uint32_t addr = suffix;
        for (const auto& c : commands) {
            set_command(rdram, addr, c[0], c[1]);
            addr += 8;
        }
        set_command(rdram, fullsync_addr, 0xDE010000, suffix);
    }

    bool is_prefix(uint32_t addr) {
        return addr >= prefix_area && addr < prefix_area + prefix_slots * prefix_bytes;
    }

    // Interpolation IDs (src/game/interpolation.cpp). Every modelview load is
    // replaced with a call to a short list holding a matrix group (its ID, or
    // automatic) and the original load, so no load inherits another's group.
    constexpr uint32_t interp_area = 0x80700000;
    constexpr uint32_t interp_banks = 4;
    constexpr uint32_t interp_bank_bytes = 0x10000;
    constexpr uint32_t interp_entry_bytes = 5 * 8;
    constexpr uint32_t interp_entries = interp_bank_bytes / interp_entry_bytes;
    constexpr uint32_t G_EX_ID_AUTO = 0xFFFFFFFF;
    uint32_t interp_bank = 0;
    uint32_t interp_entry = 0;

    bool in_interp_area(uint32_t addr) {
        return addr >= interp_area && addr < interp_area + interp_banks * interp_bank_bytes;
    }

    // gEXMatrixGroup parameters. Skeleton (limb) matrices always interpolate
    // and are blended entry by entry: the game's bone matrices are skewed, and
    // decomposing them warped limbs on in-between frames. Everything else
    // keeps RT64's automatic settings. A numbered ID is unique in the frame and
    // is paired directly (G_EX_ORDER_LINEAR), otherwise by closeness.
    uint32_t matrix_group_params(bool skeleton, bool numbered) {
        constexpr uint32_t interpolate = 1, automatic = 2;
        const uint32_t motion = skeleton ? interpolate : automatic;
        const uint32_t order = numbered ? 0u : 1u;
        return (0u << 0) |   // no push
            (0u << 1) |      // modelview
            ((skeleton ? 0u : 1u) << 2) | // decompose
            (motion << 3) |  // position
            (motion << 5) |  // rotation
            (motion << 7) |  // scale
            (motion << 9) |  // skew
            (motion << 11) | // perspective
            (0u << 13) |     // vertices: skip
            (motion << 15) | // tiles
            (order << 17) |
            (0u << 19) |     // not editable
            (0u << 20) |     // aspect: auto
            (0u << 22) |     // texture coordinates: skip
            (automatic << 24); // look-at
    }

    // Matrix loads this frame that belong to an instance (a skeleton or an
    // object). Their IDs get the instance's number after the walk.
    struct InstanceTag {
        uint32_t entry;
        uint32_t id;
        uint32_t instance;
        bool object;
    };
    std::vector<InstanceTag> instance_tags;

    void tag_matrix(uint8_t* rdram, uint32_t addr, uint32_t w0, uint32_t w1) {
        if (interp_entry >= interp_entries) {
            return;
        }
        uint32_t instance = 0;
        bool object = false;
        uint32_t id = snp::interp_matrix_id(resolve(w1), instance, object);
        uint32_t entry = interp_area + interp_bank * interp_bank_bytes + interp_entry * interp_entry_bytes;
        const uint32_t commands[5][2] = {
            { interp_entry == 0 ? 0xE0525464u : G_NOOP << 24, interp_entry == 0 ? 0x10000064u : 0u }, // gEXEnable, once a frame
            { 0x6400000C, id }, // gEXMatrixGroup
            { matrix_group_params(id != G_EX_ID_AUTO && !object, false), 0 },
            { w0, w1 }, // the game's matrix load
            { 0xDF000000, 0 },
        };
        for (uint32_t i = 0; i < 5; i++) {
            set_command(rdram, entry + i * 8, commands[i][0], commands[i][1]);
        }
        if (instance != 0) {
            instance_tags.push_back({ entry, id, instance, object });
        }
        interp_entry++;
        set_command(rdram, addr, 0xDE000000, entry);
    }

    // After the walk: number the instances this display list draws (all at
    // once) and put the numbers into their matrices' IDs.
    void number_tagged_instances(uint8_t* rdram) {
        if (instance_tags.empty()) {
            return;
        }
        std::vector<uint32_t> drawn;
        for (const InstanceTag& tag : instance_tags) {
            drawn.push_back(tag.instance);
        }
        std::sort(drawn.begin(), drawn.end());
        drawn.erase(std::unique(drawn.begin(), drawn.end()), drawn.end());
        snp::interp_number_instances(drawn);
        for (const InstanceTag& tag : instance_tags) {
            set_command(rdram, tag.entry + 8, 0x6400000C, snp::interp_numbered_id(tag.id, tag.instance));
            set_command(rdram, tag.entry + 16, matrix_group_params(!tag.object, true), 0);
        }
        instance_tags.clear();
    }

    // HUD text is drawn as 1:1 texture rectangles starting exactly on a texel.
    // Upscaled, edge pixels can round to the texel before it and show thin
    // lines from the neighbouring glyph; 1:1 rectangles start 1/32 texel later,
    // which samples the same texels at the N64's resolution.
    void nudge_rect_start(uint8_t* rdram, uint32_t addr) {
        uint32_t h0 = (uint32_t)MEM_W(0, (gpr)(int32_t)(addr + 8));  // RDPHALF_1: s, t (S10.5)
        uint32_t h1 = (uint32_t)MEM_W(4, (gpr)(int32_t)(addr + 8));
        uint32_t h2 = (uint32_t)MEM_W(4, (gpr)(int32_t)(addr + 16)); // RDPHALF_2: dsdx, dtdy (S5.10)
        int32_t s = (int16_t)(h1 >> 16), t = (int16_t)h1;
        if ((h2 >> 16) != 1 << 10 || (h2 & 0xFFFF) != 1 << 10 || (s & 31) != 0 || (t & 31) != 0) {
            return; // scaled, or already off a texel boundary (including a rectangle nudged before)
        }
        s += 1;
        t += 1;
        set_command(rdram, addr + 8, h0, ((uint32_t)(uint16_t)s << 16) | (uint16_t)t);
    }

    // HUD anchoring in Expand. RT64 keeps 2D rectangles in the centred 4:3
    // area unless they're anchored to an edge (gEXSetRectAlign). The HUD is
    // drawn after the game loads its 2D projection (0x802C3AE0); its top band
    // has the score on the left, TIME in the middle and LIFE on the right.
    // Elements there are anchored to the nearer edge, 16 pixels further out
    // (the original keeps a TV margin): each rectangle is replaced with a call
    // to a list that sets the alignment and a whole-window scissor, draws it
    // and restores both. Everything lower is left alone.
    constexpr uint32_t hud_projection = 0x802C3AE0;
    constexpr uint32_t hud_area = 0x807A0000;
    constexpr uint32_t hud_banks = 4;
    constexpr uint32_t hud_bank_bytes = 0x14000; // 640 entries; the banks end at 0x807F0000
    constexpr uint32_t hud_entry_max = 16; // commands; a texture rectangle needs 13
    constexpr uint32_t hud_entry_bytes = hud_entry_max * 8;
    constexpr uint32_t hud_entries = hud_bank_bytes / hud_entry_bytes;
    constexpr uint32_t hud_band_bottom = 40 * 4; // 10.2
    constexpr uint32_t hud_element_bottom = 80 * 4;
    constexpr int32_t hud_margin = 16 * 4;
    constexpr int32_t right_origin_shift = -320 * 4; // RT64 adds the width to right-anchored coordinates
    constexpr uint32_t origin_left = 0x0, origin_right = 0x400, origin_none = 0x800;
    bool in_hud = false;
    uint32_t hud_bank = 0;
    uint32_t hud_entry = 0;

    bool in_hud_area(uint32_t addr) {
        return addr >= hud_area && addr < hud_area + hud_banks * hud_bank_bytes;
    }

    // The edge an element (bounds in 10.2) is anchored to, or false to leave
    // it centred. It must start in the top band and end near it (the
    // tutorial's box around the life gauge ends at about y 50). The side goes
    // by where it starts or ends when that's clear of the middle (a shrinking
    // bar's centre moves), otherwise by its centre.
    bool hud_origin(uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry, uint32_t& origin, int32_t& offset) {
        if (uly >= hud_band_bottom || lry >= hud_element_bottom) {
            return false;
        }
        uint32_t centre = (ulx + lrx) / 2;
        if (ulx >= 176 * 4 || centre > 192 * 4) {
            origin = origin_right;
            offset = right_origin_shift + hud_margin;
            return true;
        }
        if (lrx <= 144 * 4 || centre < 128 * 4) {
            origin = origin_left;
            offset = -hud_margin;
            return true;
        }
        return false;
    }

    // addr: a G_TEXRECT (3 commands) or G_FILLRECT (1 command).
    void anchor_rect(uint8_t* rdram, uint32_t addr, uint32_t words, uint32_t origin, int32_t offset) {
        if (hud_entry >= hud_entries) {
            return;
        }
        uint32_t entry = hud_area + hud_bank * hud_bank_bytes + hud_entry * hud_entry_bytes;
        hud_entry++;
        uint32_t commands[hud_entry_max][2];
        uint32_t n = 0;
        auto cmd = [&](uint32_t a, uint32_t b) { commands[n][0] = a; commands[n][1] = b; n++; };
        cmd(0xE0525464, 0x10000064); // gEXEnable
        cmd(0x64000017, 0);          // gEXPushScissor
        cmd(0x64000005, (origin_left << 2) | (origin_right << 14)); // gEXSetScissor: the whole window
        cmd(0, 240 * 4);
        cmd(0x64000006, origin | (origin << 12)); // gEXSetRectAlign
        cmd((uint32_t)(uint16_t)offset << 16, (uint32_t)(uint16_t)offset << 16);
        for (uint32_t i = 0; i < words; i++) {
            cmd((uint32_t)MEM_W(0, (gpr)(int32_t)(addr + i * 8)), (uint32_t)MEM_W(4, (gpr)(int32_t)(addr + i * 8)));
        }
        cmd(0x64000006, origin_none | (origin_none << 12)); // back to unanchored
        cmd(0, 0);
        cmd(0x64000018, 0);          // gEXPopScissor
        cmd(0xDF000000, 0);
        for (uint32_t i = 0; i < n; i++) {
            set_command(rdram, entry + i * 8, commands[i][0], commands[i][1]);
        }
        set_command(rdram, addr, 0xDE000000, entry);
        for (uint32_t i = 1; i < words; i++) {
            set_command(rdram, addr + i * 8, G_NOOP << 24, 0);
        }
    }

    // A HUD element is a run of rectangles drawn back to back (only state
    // changes between them) that touch, directly or through others: a string's
    // glyphs, the life bar's frame and fill. Each is anchored as a whole by its
    // bounds. The life bar's highlight is drawn in 1-pixel columns, so runs
    // reach about 85 rectangles; a run that fills up is anchored in parts.
    constexpr uint32_t max_fill_run = 256;
    struct HudRect {
        uint32_t addr, words, ulx, uly, lrx, lry;
    };
    HudRect fill_run[max_fill_run];
    uint32_t fill_run_count = 0;

    // The crosshair's vertical line when the reticle is past the 4:3 picture
    // (Expand): beyond x 320 the game's scissor cuts it off, and left of 0 its
    // 12-bit x wraps to near 1024. It's drawn with the whole window as the
    // scissor, a wrapped one moved back by 1024.
    void place_crosshair_line(uint8_t* rdram, const HudRect& r) {
        if (r.words != 1 || r.lry - r.uly < 200 * 4 || r.lrx - r.ulx > 8 * 4) {
            return;
        }
        if (r.ulx >= 640 * 4) {
            anchor_rect(rdram, r.addr, 1, origin_none, -1024 * 4);
        }
        else if (r.lrx > 320 * 4) {
            anchor_rect(rdram, r.addr, 1, origin_none, 0);
        }
    }

    void end_fill_run(uint8_t* rdram) {
        uint32_t n = fill_run_count;
        fill_run_count = 0;
        uint32_t shape[max_fill_run];
        for (uint32_t i = 0; i < n; i++) {
            shape[i] = i;
        }
        auto find = [&](uint32_t i) { while (shape[i] != i) i = shape[i] = shape[shape[i]]; return i; };
        constexpr uint32_t touch = 1 * 4;
        // The crosshair's full-height and full-width lines join nothing, or
        // crossing the HUD they'd make it one element too big to anchor.
        auto joins = [&](const HudRect& r) {
            return r.uly < hud_band_bottom && r.lry < hud_element_bottom && r.lrx - r.ulx < 240 * 4;
        };
        for (uint32_t i = 0; i < n; i++) {
            for (uint32_t j = i + 1; j < n; j++) {
                const HudRect& a = fill_run[i];
                const HudRect& b = fill_run[j];
                if (joins(a) && joins(b) && a.ulx <= b.lrx + touch && b.ulx <= a.lrx + touch && a.uly <= b.lry + touch && b.uly <= a.lry + touch) {
                    shape[find(i)] = find(j);
                }
            }
        }
        for (uint32_t i = 0; i < n; i++) {
            place_crosshair_line(rdram, fill_run[i]);
        }
        for (uint32_t i = 0; i < n; i++) {
            if (find(i) != i) {
                continue;
            }
            uint32_t ulx = 0xFFF, uly = 0xFFF, lrx = 0, lry = 0;
            for (uint32_t j = 0; j < n; j++) {
                if (find(j) == i) {
                    ulx = std::min(ulx, fill_run[j].ulx);
                    uly = std::min(uly, fill_run[j].uly);
                    lrx = std::max(lrx, fill_run[j].lrx);
                    lry = std::max(lry, fill_run[j].lry);
                }
            }
            uint32_t origin;
            int32_t offset;
            if (!hud_origin(ulx, uly, lrx, lry, origin, offset)) {
                continue;
            }
            for (uint32_t j = 0; j < n; j++) {
                if (find(j) == i) {
                    anchor_rect(rdram, fill_run[j].addr, fill_run[j].words, origin, offset);
                }
            }
        }
    }

    void add_hud_rect(uint8_t* rdram, uint32_t addr, uint32_t w0, uint32_t w1, uint32_t words) {
        if (fill_run_count >= max_fill_run) {
            end_fill_run(rdram);
        }
        fill_run[fill_run_count++] = { addr, words, (w1 >> 12) & 0xFFF, w1 & 0xFFF, (w0 >> 12) & 0xFFF, w0 & 0xFFF };
    }

    // Commands that may come between an element's rectangles.
    bool is_fill_run_state(uint32_t op) {
        switch (op) {
            case 0xE2: case 0xE3: case 0xE6: case 0xE7: case 0xE8: // othermode, syncs
            case 0xF7: case 0xFA: case 0xFB: case 0xFC:              // colours, combine
            case 0xFD: case 0xF5: case 0xF4: case 0xF3: case 0xF2: case 0xF0: // textures, tiles
            case 0xE1: case 0xF1:                                   // texture rectangle halves
                return true;
            default:
                return false;
        }
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
            case 0xDB: printf("[DL] %*smoveword %08X %08X\n", depth, "", w0, w1); break;
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

    // top: the list is the frame's own, or one it branched to (its end ends the frame).
    void walk(uint8_t* rdram, uint32_t addr, int depth, bool widen, bool top) {
        for (int n = 0; n < 200000; n++, addr += 8) {
            uint32_t w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
            uint32_t w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)addr);
            if ((w0 >> 24) == 0xDE && in_hud_area(w1)) {
                // A HUD rectangle anchored in an earlier frame (a list the game
                // keeps): back to the game's command(s), anchored again below.
                // They're at entry + 6 commands (anchor_rect).
                uint32_t entry = w1 + 6 * 8;
                uint32_t words = ((uint32_t)MEM_W(0, (gpr)(int32_t)entry) >> 24) == 0xE4 ? 3 : 1;
                for (uint32_t i = 0; i < words; i++) {
                    set_command(rdram, addr + i * 8, (uint32_t)MEM_W(0, (gpr)(int32_t)(entry + i * 8)), (uint32_t)MEM_W(4, (gpr)(int32_t)(entry + i * 8)));
                }
                w0 = (uint32_t)MEM_W(0, (gpr)(int32_t)addr);
                w1 = (uint32_t)MEM_W(4, (gpr)(int32_t)addr);
            }
            if ((w0 >> 24) == 0xDE && in_interp_area(w1)) {
                // A matrix load tagged in an earlier frame (a list the game
                // keeps): back to the game's command, tagged again below.
                uint32_t entry = w1;
                w0 = (uint32_t)MEM_W(3 * 8, (gpr)(int32_t)entry);
                w1 = (uint32_t)MEM_W(3 * 8 + 4, (gpr)(int32_t)entry);
                set_command(rdram, addr, w0, w1);
            }
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
                    if ((w0 & 0xFFF) + 1 == 320) {
                        bool known = false;
                        for (uint32_t i = 0; i < cimg_count; i++) {
                            known = known || cimgs[i][1] == w1;
                        }
                        if (!known && cimg_count < max_cimgs) {
                            cimgs[cimg_count][0] = w0;
                            cimgs[cimg_count][1] = w1;
                            cimg_count++;
                        }
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
            if (fill_run_count > 0 && (w0 >> 24) != G_FILLRECT && (w0 >> 24) != 0xE4 && !is_fill_run_state(w0 >> 24)) {
                end_fill_run(rdram);
            }
            switch (w0 >> 24) {
                case 0xDE: { // G_DL
                    bool branch = ((w0 >> 16) & 0xFF) != 0; // no return
                    if (depth < 10) {
                        walk(rdram, resolve(w1), depth + 1, widen, top && branch);
                    }
                    if (branch) {
                        return;
                    }
                    break;
                }
                case 0xDF: // G_ENDDL
                    return;
                case 0xE9: // G_RDPFULLSYNC: the end of the frame's drawing
                    if (top && widen && starts_with_picture && effect_area_scissor && first_cimg_w0 != 0 &&
                        !(addr >= suffix_area && addr < suffix_area + suffix_slots * suffix_bytes)) {
                        black_edge_rows(rdram, addr);
                        return;
                    }
                    break;
                case 0xED: // G_SETSCISSOR
                    if (w0 == effect_scissor_w0 && w1 == effect_scissor_w1) {
                        effect_area_scissor = true;
                    }
                    break;
                case 0xDB: // G_MOVEWORD
                    if (((w0 >> 16) & 0xFF) == 6) { // G_MW_SEGMENT
                        segments[((w0 & 0xFFFF) / 4) & 0xF] = w1 & 0x00FFFFFF;
                    }
                    break;
                case 0xE4: // G_TEXRECT: two more words follow (RDPHALF_1, RDPHALF_2)
                    nudge_rect_start(rdram, addr);
                    if (widen && in_hud) {
                        add_hud_rect(rdram, addr, w0, w1, 3);
                    }
                    addr += 16;
                    break;
                case 0xF6: // G_FILLRECT
                    if (widen && in_hud) {
                        add_hud_rect(rdram, addr, w0, w1, 1);
                    }
                    break;
                case 0xDA: // G_MTX
                    if ((((w0 & 0xFF) ^ 1) & 4) != 0) { // projection
                        in_hud = (w1 == hud_projection);
                    }
                    else { // modelview
                        tag_matrix(rdram, addr, w0, w1);
                    }
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
        dl = (uint32_t)MEM_W((prefix_commands - 1) * 8 + 4, (gpr)(int32_t)dl);
        MEM_W(0x30, task) = (int32_t)dl;
    }
    bool widen = snp::get_widescreen_scale() > 1.0f;
    interp_bank = (interp_bank + 1) % interp_banks;
    interp_entry = 0;
    snp::interp_new_frame();
    if (log_frame) {
        static const auto start = std::chrono::steady_clock::now();
        double t = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
        printf("[DL] ---- frame %d at %.1f s, display list %08X%s\n", frame - 1, t, dl, widen ? " (widened)" : "");
    }
    for (uint32_t& s : segments) {
        s = 0;
    }
    first_cimg_w0 = 0;
    cimg_count = 0;
    seen_draw = false;
    starts_with_picture = false;
    effect_area_scissor = false;
    in_hud = false;
    hud_bank = (hud_bank + 1) % hud_banks;
    hud_entry = 0;
    fill_run_count = 0;
    instance_tags.clear();
    walk(rdram, dl, 0, widen, true);
    end_fill_run(rdram);
    number_tagged_instances(rdram);
    // After switching from Expand back to Original, the strips outside the
    // inset keep the wide picture (the game never clears them): the first
    // frames clear every color image they draw to, enough for all three
    // framebuffers.
    static bool was_widened = false;
    static int clear_after_switch = 0;
    if (was_widened && !widen) {
        clear_after_switch = 6;
    }
    was_widened = widen;
    bool clear_all = clear_after_switch > 0;
    if (clear_all) {
        clear_after_switch--;
    }
    if (((widen && starts_with_picture) || clear_all) && first_cimg_w0 != 0) {
        MEM_W(0x30, task) = (int32_t)clear_prefix(rdram, dl, clear_all);
    }
    if (log_frame) {
        fflush(stdout);
    }
}
