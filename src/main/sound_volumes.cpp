// Music Volume and Sound Effects Volume on the Sound tab.
//
// The game plays its music and sound effects through the same sound engine,
// as sounds with channels (0x13C bytes each, from 0x8033FF90). Each channel
// keeps the handle of the sound it belongs to at 0x44; the music is started
// by func_8003C260, which keeps its handle at 0x80074540. The hook in
// snp.jp.toml (func_80054B64) scales each channel's volume, before it's
// given to its voice, by Music Volume when it's the music's and by Sound
// Effects Volume otherwise.
#include "snp_config.h"
#include "snp_recomp_hooks.h"

#include "recompui/config.h"

namespace {
    const std::string music_id = "music_volume";
    const std::string effects_id = "sound_effects_volume";
    bool options_added = false;

    constexpr uint32_t music_handle_addr = 0x80074540;
    // Still the music's while it fades out: the game clears its handle when
    // it starts the fade.
    uint32_t last_music_handle = 0;

    uint32_t read_word(uint8_t* rdram, uint32_t vram) {
        return *reinterpret_cast<uint32_t*>(rdram + (vram - 0x80000000u));
    }
}

void snp::config::add_sound_volume_options(recomp::config::Config& sound) {
    options_added = true;
    sound.add_percent_number_option(music_id, "Music Volume", "Controls the volume of the music.", 100.0);
    sound.add_percent_number_option(effects_id, "Sound Effects Volume",
        "Controls the volume of the sound effects and voices.", 100.0);
}

extern "C" uint32_t snp_track_volume(uint8_t* rdram, uint32_t track, uint32_t volume) {
    if (!options_added) {
        return volume;
    }
    uint32_t music_handle = read_word(rdram, music_handle_addr);
    if (music_handle != 0) {
        last_music_handle = music_handle;
    }
    uint32_t handle = read_word(rdram, track + 0x44);
    bool music = handle != 0 && handle == last_music_handle;
    double percent = std::get<double>(recompui::config::get_sound_config().get_option_value(music ? music_id : effects_id));
    if (percent >= 100.0) {
        return volume;
    }
    if (percent <= 0.0) {
        return 0;
    }
    return (uint32_t)(volume * (percent / 100.0));
}
