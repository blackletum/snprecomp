// Which of the two supported ROMs is running (src/main/main.cpp's
// supported_games). They share one recompiled build; the hooks in
// snp.jp.toml use this to substitute the few constants the English fan
// translation changed.
#include "librecomp/game.hpp"

#include "snp_recomp_hooks.h"

extern "C" int snp_rom_is_english(void) {
    // The game id can't change while the game runs, so look it up once.
    static const bool english = recomp::current_game_id() == u8"snp.n64.en.fan";
    return english ? 1 : 0;
}
