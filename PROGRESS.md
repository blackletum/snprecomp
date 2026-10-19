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

## Next

- Past the attract mode: input, the title menu, gameplay, sound.
- Confirm the EEPROM size (4K assumed).
- Rumble, controller defaults for the game's controls.
- Higher framerates: check the game's frame timing (btgarecomp round 138's
  clock-rate issue) and RT64 interpolation matching.
- Widescreen: HUD anchoring, the crosshair's screen-to-world mapping, and
  culling at the screen edges.
- For modders: texture packs from the mods folder, a texture dump switch,
  the list of text textures that differ between the ROMs, and the symbol
  file with names as functions are identified.

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
