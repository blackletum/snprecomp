// Other dumps and patches of the game. Translations change only a few
// constants in the code (addresses of data they moved), so any ROM whose code
// matches the Japanese release's elsewhere runs this one recompiled build:
// the launcher accepts it (snp_rom_check), and the hooks in snp.jp.toml put
// the loaded ROM's own constants in place of the Japanese ones (snp_rom_value).
#include <cstdint>
#include <cstring>
#include <string_view>
#include <vector>

#include "librecomp/game.hpp"

#include "snp_recomp_hooks.h"

namespace {
    constexpr uint32_t main_vram = 0x80025C00;
    constexpr uint32_t main_rom = 0x1000;
    constexpr uint32_t main_text_size = 0x31280;
    constexpr uint32_t load_table_rom = 0x34BAC; // the game's segment load table

    // Words translations change: the boot code's first four and the constants
    // the hooks correct.
    constexpr uint32_t varying_words[] = {
        0x80025C00, 0x80025C04, 0x80025C08, 0x80025C0C,
        0x8002BD5C, 0x8002BD8C, 0x8002BD98,
        0x8003C158, 0x8003C15C, 0x8003C198, 0x8003C19C, 0x8003C1EC, 0x8003C1F0,
    };

    // The fingerprint of the Japanese release's code (tools/rom_fingerprint.py).
    constexpr uint64_t code_fingerprint = 0x087c5eaa445375ebull;

    uint64_t fnv1a(const uint8_t* data, size_t size, uint64_t h) {
        for (size_t i = 0; i < size; i++) {
            h = (h ^ data[i]) * 0x100000001b3ull;
        }
        return h;
    }

    uint32_t read_be32(const std::vector<uint8_t>& rom, size_t offset) {
        return (uint32_t)rom[offset] << 24 | (uint32_t)rom[offset + 1] << 16 | (uint32_t)rom[offset + 2] << 8 | rom[offset + 3];
    }

    // FNV-1a over the main code (varying words zeroed), the load table and
    // every segment's code it lists.
    bool code_matches(const std::vector<uint8_t>& rom) {
        if (rom.size() < load_table_rom + 20) {
            return false;
        }
        std::vector<uint8_t> main(rom.begin() + main_rom, rom.begin() + main_rom + main_text_size);
        for (uint32_t vram : varying_words) {
            std::memset(main.data() + (vram - main_vram), 0, 4);
        }
        uint64_t h = fnv1a(main.data(), main.size(), 0xcbf29ce484222325ull);
        for (size_t p = load_table_rom; p + 20 <= rom.size(); p += 20) {
            h = fnv1a(rom.data() + p, 20, h);
            uint32_t seg_rom = read_be32(rom, p), seg_vram = read_be32(rom, p + 4), text_end = read_be32(rom, p + 8);
            if (seg_rom == 0 || seg_vram == 0) {
                return h == code_fingerprint;
            }
            if (text_end < seg_vram || (uint64_t)seg_rom + (text_end - seg_vram) > rom.size()) {
                return false;
            }
            h = fnv1a(rom.data() + seg_rom, text_end - seg_vram, h);
        }
        return false;
    }

    bool code_checked = false;
    bool last_match = false;
    const std::vector<uint8_t>* last_rom = nullptr;
    size_t last_size = 0;
}

// The launcher's check for ROMs whose hash isn't one of supported_games
// (src/main/main.cpp): a ROM with the game's code is the Japanese entry if its
// header names the Japanese release, the English one otherwise.
bool snp_rom_check(const recomp::GameEntry& game, const std::vector<uint8_t>& rom) {
    // The launcher asks once per entry for the same ROM.
    if (!code_checked || last_rom != &rom || last_size != rom.size()) {
        last_match = code_matches(rom);
        code_checked = true;
        last_rom = &rom;
        last_size = rom.size();
    }
    if (!last_match) {
        return false;
    }
    bool japanese = std::string_view(reinterpret_cast<const char*>(rom.data()) + 0x20, 14) == "TSUMI TO BATSU";
    return japanese == (game.game_id == u8"snp.n64.jp.1.0");
}

// value: what the Japanese code's lui (at lui_addr, the word lui_jp) and
// addiu/ori (at lo_addr, lo_jp) built; either address may be 0 for none.
// Returns what the loaded ROM's own instructions there build.
extern "C" uint32_t snp_rom_value(uint8_t* rdram, uint32_t value, uint32_t lui_addr, uint32_t lui_jp, uint32_t lo_addr, uint32_t lo_jp) {
    auto word = [&](uint32_t addr) { return *(uint32_t*)(rdram + (addr - 0x80000000u)); };
    if (lui_addr != 0) {
        value += (word(lui_addr) - lui_jp) << 16;
    }
    if (lo_addr != 0) {
        value += (uint32_t)(int32_t)(int16_t)word(lo_addr) - (uint32_t)(int32_t)(int16_t)lo_jp;
    }
    return value;
}
