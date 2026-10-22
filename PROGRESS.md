# Progress / Roadmap

Where the port stands. STATUS.md has the full history of how each piece was
found and fixed.

The goal is the original game, running natively: fixes are limited to what
the PC port needs (timing, rendering and runtime differences from the N64),
and leave the game's own behaviour, look and balance alone.

## Done

- **ROM analysis**: memory map, the game's segment load table (main, seg0
  and 26 code overlays), the SDK libraries and the audio microcode
  (`syms/rom_info.md`, STATUS.md round 1).
- **Both ROMs**: the Japanese ROM and the fan English translation share one
  recompiled build. Their code differs only in two functions' constants,
  substituted by hooks when the English ROM runs.
- **Recompilation**: all 6280 functions recompile from the Japanese ROM
  (`snp.jp.toml`, `SnPSyms/`), generated reproducibly by `tools/`.
- **Audio microcode**: recompiled by RSPRecomp (`n_aspMain.jp.toml`).
- **Project**: build, frontend, patch pipeline and RT64 fixes set up from
  btgarecomp.
- **Boots**: both ROMs run the attract mode (intro cutscenes, "PRESS START
  BUTTON", subtitles in the ROM's language). Busy-wait loops yield to the
  runtime's scheduler and overlays are registered as the game loads them
  (STATUS.md round 2).
- **Plays**: the title screen, main menu and stage 1 run. Division by zero
  gives the N64's results instead of crashing (an N64Recomp fix,
  `lib-patches/N64Recomp/`, STATUS.md round 3).
- **Audio**: no crackle; the audio thread gets hardware-like AI length and
  FIFO status (STATUS.md round 6). Quitting can't hang on Windows.

## Next

- Play further: the rest of stage 1 and the later stages, the other menu
  options, saving.
- Confirm the EEPROM size (4K assumed).
- Rumble, controller defaults for the game's controls.
- Widescreen: the second-long zoom into the title (drawn by the CPU, stays
  4:3); a few scenes show sky or nothing past the 4:3 view's edges (the
  entry scene's corners, the elevator's top corner: no geometry there).
  (Done: the picture widens, STATUS.md round 5; fades, tints and the title
  screen, round 7; culling at the edges, round 8; HUD anchoring, round 9 and
  after; the reticle reaching the sides.)
- Higher framerates: characters, swarms and alike objects are paired by
  stable IDs (confirmed: soldiers, insects, cones); still to see: a boss's
  fire wave and energy fountains.
- The in-game pause: a looping effect sound in stage 2, taken as the game's
  own (closed).
- Texture packs: RT64 packs from the Mods menu; Rice Video packs from
  `texture_packs`, matched at runtime by Rice's own texture keys
  (lib-patches/rt64/0020; Rice read a 4-bit texture's palette at twice its
  offset, and packs are named by that). Tested with Vanit's English pack.
- On hold: for modders, a texture dump switch, the list of text textures that differ between the ROMs, and the
  symbol file with names as functions are identified.

## Where things are

| Path | What |
|---|---|
| `snp.jp.toml` | N64Recomp config: hooks, instruction patches |
| `SnPSyms/` | Symbol file (function boundaries), generated |
| `n_aspMain.jp.toml` | RSP audio microcode config |
| `src/main/` | Frontend: window, audio, input, config, crash reporting |
| `src/game/` | Game-specific code called from hooks |
| `patches/` | Whole-function replacements compiled for MIPS |
| `assets/` | UI font, icons and their licenses |
| `licenses/` | License notices for shipped files not in a submodule |
| `lib-patches/` | Patches applied to submodules at configure time |
| `tools/` | ROM analysis, symbol file generation, release packaging |
| `syms/` | ROM notes and the raw function lists the symbol file is built from |
