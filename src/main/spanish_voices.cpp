// Spanish Voices on the Enhancements tab: the Spanish dub's voices with the
// loaded ROM's text.
//
// The Spanish translation (IlDucci, PacoChan, Zoinkity and others, 2015)
// changes the game's code to show its text, so this port can't run it. Its
// dub's voices are only sound data, though. Load Spanish ROM keeps that
// sound data (and the voice banks' offsets in it) in the app's folder; when
// the game starts with Spanish Voices on, it's added past the end of the
// loaded ROM, and what finds the sound data is pointed at it: the four
// instructions that build the first wave table's ROM offset (read through
// the hooks in snp.jp.toml) and the table of the voice banks' ROM offsets in
// the main program's data.
#include "snp_config.h"

#include <algorithm>
#include <cstring>
#include <span>
#include <filesystem>
#include <fstream>
#include <vector>

#include "librecomp/game.hpp"
#include "recompui/config.h"
#include "recompui/recompui.h"
#include "util/file.h"

namespace {
    const std::string enhancements_id = "enhancements";
    const std::string option_id = "spanish_voices";
    bool option_added = false;

    // In the Spanish dub (SAPESP.10.DOBLADO.xdelta on the Japanese ROM).
    constexpr uint32_t dub_sound_start = 0x0138D310; // its "N64 PTR by matsu" header
    constexpr uint32_t dub_sound_end = 0x01FF2F80;
    // In every ROM this port runs (main code and data are the Japanese release's).
    constexpr uint32_t sound_start = 0x013C2AF0;
    constexpr uint32_t sound_end = 0x01FF3000;
    constexpr uint32_t first_table = 0x130; // the first wave table, from the sound data's start
    constexpr uint32_t voice_table_rom = 0x399A8;
    constexpr uint32_t voice_banks = 40;
    constexpr uint32_t main_vram = 0x80025C00;
    constexpr uint32_t main_rom = 0x1000;
    constexpr uint32_t lui_addiu_pairs[4][2] = {
        { 0x8003AB28, 0x8003AB2C }, { 0x8003AB78, 0x8003AB7C }, { 0x8003ABA4, 0x8003ABA8 }, { 0x8003ABF0, 0x8003ABF4 },
    };

    // Every 4096th byte's run of 16 over the dub's sound data, to tell it
    // from the subtitled version's (the original voices).
    constexpr uint64_t dubbed_fingerprint = 0x7fc0cee5a586e4daull;
    constexpr uint64_t subtitled_fingerprint = 0x4dbdd4c5652ef863ull;

    // The saved file: magic, the voice banks' offsets from the sound data's
    // start, then the sound data.
    constexpr char magic[8] = { 'S', 'N', 'P', 'V', 'O', 'X', '1', 0 };

    std::filesystem::path voices_path() {
        return recompui::file::get_app_folder_path() / "spanish_voices.bin";
    }

    uint32_t read_be32(const uint8_t* p) {
        return (uint32_t)p[0] << 24 | (uint32_t)p[1] << 16 | (uint32_t)p[2] << 8 | p[3];
    }

    void write_be32(uint8_t* p, uint32_t v) {
        p[0] = (uint8_t)(v >> 24);
        p[1] = (uint8_t)(v >> 16);
        p[2] = (uint8_t)(v >> 8);
        p[3] = (uint8_t)v;
    }

    // To big-endian (.z64) order; false if it isn't an N64 ROM.
    bool normalize(std::vector<uint8_t>& rom) {
        if (rom.size() < 0x40) {
            return false;
        }
        uint32_t first = read_be32(rom.data());
        if (first == 0x80371240) {
            return true;
        }
        if (first == 0x37804012) { // .v64: 16-bit words swapped
            for (size_t i = 0; i + 1 < rom.size(); i += 2) {
                std::swap(rom[i], rom[i + 1]);
            }
            return true;
        }
        if (first == 0x40123780) { // .n64: little-endian words
            for (size_t i = 0; i + 3 < rom.size(); i += 4) {
                std::swap(rom[i], rom[i + 3]);
                std::swap(rom[i + 1], rom[i + 2]);
            }
            return true;
        }
        return false;
    }

    uint64_t sound_fingerprint(const std::vector<uint8_t>& rom) {
        uint64_t h = 0xcbf29ce484222325ull;
        for (uint32_t i = dub_sound_start; i < dub_sound_end; i += 0x1000) {
            for (uint32_t j = i; j < i + 16 && j < dub_sound_end; j++) {
                h = (h ^ rom[j]) * 0x100000001b3ull;
            }
        }
        return h;
    }

    // Load Spanish ROM: keeps the dub's sound data and voice bank offsets.
    void load_dub() {
        recompui::file::open_file_dialog([](bool success, const std::filesystem::path& path) {
            if (!success) {
                return;
            }
            std::vector<uint8_t> rom;
            {
                std::ifstream file(path, std::ios::binary);
                if (!file.good()) {
                    recompui::message_box("Failed to open the ROM file.");
                    return;
                }
                rom.assign(std::istreambuf_iterator<char>(file), {});
            }
            if (!normalize(rom) || rom.size() < dub_sound_end ||
                std::memcmp(rom.data() + dub_sound_start + 0x10, "N64 PTR by matsu", 16) != 0) {
                recompui::message_box(
                    "This isn't the Spanish dub of Sin and Punishment.\n"
                    "Choose the Japanese ROM patched with SAPESP.10.DOBLADO.xdelta.");
                return;
            }
            uint64_t fingerprint = sound_fingerprint(rom);
            if (fingerprint == subtitled_fingerprint) {
                recompui::message_box(
                    "This is the subtitled Spanish version, which keeps the original voices.\n"
                    "Choose the dubbed one (SAPESP.10.DOBLADO.xdelta) instead.");
                return;
            }
            if (fingerprint != dubbed_fingerprint) {
                recompui::message_box(
                    "This isn't the Spanish dub of Sin and Punishment.\n"
                    "Choose the Japanese ROM patched with SAPESP.10.DOBLADO.xdelta.");
                return;
            }
            std::vector<uint8_t> out(magic, magic + sizeof(magic));
            out.resize(sizeof(magic) + voice_banks * 4);
            for (uint32_t i = 0; i < voice_banks; i++) {
                uint32_t bank = read_be32(rom.data() + voice_table_rom + i * 4);
                if (bank < dub_sound_start || bank >= dub_sound_end) {
                    recompui::message_box("This Spanish dub's voice table isn't where it should be.");
                    return;
                }
                write_be32(out.data() + sizeof(magic) + i * 4, bank - dub_sound_start);
            }
            out.insert(out.end(), rom.begin() + dub_sound_start, rom.begin() + dub_sound_end);
            std::error_code ec;
            std::filesystem::create_directories(voices_path().parent_path(), ec);
            std::ofstream file(voices_path(), std::ios::binary);
            file.write(reinterpret_cast<const char*>(out.data()), (std::streamsize)out.size());
            if (!file.good()) {
                recompui::message_box("Failed to save the Spanish voices.");
                return;
            }
            recompui::message_box("The Spanish voices are ready. Turn on Spanish Voices to use them.");
        });
    }

    bool enabled() {
        return option_added &&
            std::get<bool>(recompui::config::get_config(enhancements_id).get_option_value(option_id));
    }
}

void snp::config::add_spanish_voices_option(recomp::config::Config& enhancements) {
    option_added = true;
    enhancements.add_bool_option(
        option_id,
        "Spanish Voices",
        "Plays the Spanish dub's voices (IlDucci and the Spanish translation team, 2015), with the "
        "loaded ROM's text. The Spanish translation itself changes the game's code, so it can't run "
        "here.<br/><br/>"
        "First press Load Spanish ROM and choose the Japanese ROM patched with "
        "SAPESP.10.DOBLADO.xdelta. Its voices are kept, so this is only needed once."
        "<br/><br/>"
        "Can only be changed before the game starts.",
        false
    );
    recompui::config::add_option_button(option_id, "Load Spanish ROM", load_dub);
}

void snp::config::lock_spanish_voices_option() {
    if (option_added) {
        recompui::config::get_config(enhancements_id).update_option_disabled(option_id, true);
    }
}

namespace snp {
    void apply_spanish_voices(uint8_t* rdram, recomp_context* context);
}

// The game entries' on_init_callback: after the ROM is loaded and its first
// megabyte (main code and data) copied to RDRAM, before the game runs.
void snp::apply_spanish_voices(uint8_t* rdram, recomp_context*) {
    if (!enabled()) {
        return;
    }
    std::vector<uint8_t> saved;
    {
        std::ifstream file(voices_path(), std::ios::binary);
        if (!file.good()) {
            return;
        }
        saved.assign(std::istreambuf_iterator<char>(file), {});
    }
    const size_t header = sizeof(magic) + voice_banks * 4;
    if (saved.size() <= header || std::memcmp(saved.data(), magic, sizeof(magic)) != 0) {
        return;
    }
    std::span<const uint8_t> loaded = recomp::get_rom();
    // The loaded ROM's own voice table has to be where this one's is.
    for (uint32_t i = 0; i < voice_banks; i++) {
        uint32_t bank = read_be32(loaded.data() + voice_table_rom + i * 4);
        if (bank < sound_start || bank >= sound_end) {
            return;
        }
    }
    const uint32_t new_start = std::max<uint32_t>(0x02000000, (uint32_t)((loaded.size() + 0xFFFFF) & ~size_t(0xFFFFF)));
    std::vector<uint8_t> rom(loaded.begin(), loaded.end());
    rom.resize(new_start);
    rom.insert(rom.end(), saved.begin() + header, saved.end());
    rom.resize((rom.size() + 0xFFFFF) & ~size_t(0xFFFFF));

    // A word of the main program, in the ROM and its copy in RDRAM.
    auto set_word = [&](uint32_t vram, uint32_t value) {
        write_be32(rom.data() + (vram - main_vram + main_rom), value);
        *reinterpret_cast<uint32_t*>(rdram + (vram - 0x80000000u)) = value;
    };
    const uint32_t table = new_start + first_table;
    const uint32_t hi = (table + 0x8000) >> 16;
    const uint32_t lo = table & 0xFFFF;
    for (const auto& pair : lui_addiu_pairs) {
        uint32_t lui = read_be32(rom.data() + (pair[0] - main_vram + main_rom));
        uint32_t addiu = read_be32(rom.data() + (pair[1] - main_vram + main_rom));
        set_word(pair[0], (lui & 0xFFFF0000) | hi);
        set_word(pair[1], (addiu & 0xFFFF0000) | lo);
    }
    for (uint32_t i = 0; i < voice_banks; i++) {
        uint32_t offset = read_be32(saved.data() + sizeof(magic) + i * 4);
        set_word(main_vram + (voice_table_rom - main_rom) + i * 4, new_start + offset);
    }
    recomp::set_rom_contents(std::move(rom));
}
