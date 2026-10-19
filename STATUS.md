# Status log

How each piece of the port was found and fixed, newest last. PROGRESS.md is
the short version.

## Round 1: ROM analysis, symbol file, first recompile, project setup

### The two ROMs

The Japanese ROM and the "(U) [!]" English ROM are 32 MiB each and 62.9%
identical byte for byte. The English one is a fan translation, not an
official build (details in `syms/rom_info.md`): a ROM tool's boot stub at
`0x1FFFF70`, the header changed to `NGUE`, and the text done by swapping
textures in the asset data, which moved everything after the changed files
up by about 155 KB.

All code segments are identical. Outside the entry point (which the boot
stub overwrote and the recompiled build doesn't use), the only code
differences are constants pointing at moved data:

| Function | Instructions | Japanese | English |
|---|---|---|---|
| `func_8002BCE0` | `0x8002BD5C`, `0x8002BD8C`, `0x8002BD98` | `0x802B5D20` | `0x802B5D60` |
| `func_8003C0D0` | lui/addiu at `0x8003C158`, `0x8003C198`, `0x8003C1EC` | `0x00A8CA40` | `0x00A65CF0` |

The second is a ROM offset, the end of the asset data. The asset table at
ROM `0x34DF4` is data, so the game reads the right one from whichever ROM is
loaded.

So both ROMs run one recompiled build, generated from the Japanese ROM.
Six `[[patches.hook]]` entries in `snp.jp.toml` put the English values in
the register after each of those instruction pairs when the English ROM is
running (`src/game/rom_version.cpp`, from the runtime's game id). Both ROMs
are registered in `src/main/main.cpp` with their own game id, so their saves
are separate.

### Memory layout

- IPL3 loads ROM `0x1000` to the entry point `0x80025C00`
  (`rom = vram - 0x80024C00`). The entry point clears BSS
  `0x80063450`-`0x800A7070`, sets the stack and jumps to `0x80025C40`, which
  calls `__osInitialize_common` and starts the first thread
  (`osCreateThread` `0x80047270`, `osStartThread` `0x80047510`).
- Main's text ends at `0x80056E80`. The last function is the Yay0 decoder at
  `0x80056DD0`, so some assets are Yay0-compressed. After it come rspboot
  (`0x80056E80`), the audio microcode (`0x80056F50`) and a graphics
  microcode (`0x80057BB0`), then data.
- The game's own load table, at ROM `0x34BAC` (VRAM `0x800597AC`), lists
  28 segments as {rom start, vram, text end, data end, bss end}: seg0 at
  `0x800A7070` (right after main's BSS), overlays 1-23 all at `0x800E4780`,
  24-27 at `0x802B4000` / `0x802C3B30` (24 has no code). The table ends at
  an entry with vram 0. `tools/segments.py` reads it.

### libultra and the other SDK libraries

[n64sym](https://github.com/shygoo/n64sym)'s built-in signatures name 226
functions in main (`syms/n64sym_jp.txt`): libultra (threads, message queues,
PI/SI/VI/AI managers, EEPROM, Controller Pak, Rumble Pak), libsched's
`osCreateScheduler`/`__sc*`, the `al*` and `n_al*` audio libraries, gu
matrix helpers and libgcc's 64-bit division. All but two land on a function
start spimdisasm found; `__osException`'s tail and `__osCleanupThread` were
added as starts. N64Recomp's built-in lists already cover all of the names
that need replacing or skipping, so `snp.jp.toml` doesn't list any.

The game saves to EEPROM (`osEepromLongRead`/`Write` are linked and
called). The game entries use `SaveType::Eep4k`; whether the game wants 4K
or 16K isn't confirmed yet, check what it does with `osEepromProbe`'s
result.

### Function boundaries

`tools/run_spimdisasm.py` runs spimdisasm's `singleFileDisasm` on each
segment's text (main, seg0, 26 overlays with code) and keeps its function
list (`syms/spimdisasm/*.csv`). `tools/gen_syms_toml.py` turns those and the
n64sym names into `SnPSyms/snp.jp.syms.toml`: 6278 functions, one
`[[section]]` per segment. Overlay functions are named `func_ovlNN_XXXXXXXX`
since overlays share addresses.

N64Recomp can't branch into another function's interior. Hand-written
code does that, for shared tails: `__osException` (its interrupt dispatch
tail was a separate function, kept as one piece by an override) and libgcc's
float conversions at `0x8004BDD0`-`0x8004C2F8`. The generator fixes these:
it splits a function at the target when the code before it never falls
through, compiles a self-contained tail (code up to its first unconditional
exit, with no branches leaving it) as its own small function so branches to
it become tail calls, and otherwise merges the two functions. An entry
point that disappears into a merged function but is called from elsewhere
stays a function of its own, overlapping the merged one and running to its
end; the only one is `func_8004BE70` (called 288 times), whose branch back
to `0x8004BE30` goes to such a tail.

(N64Recomp's `manual_funcs` would be the usual way to add an overlapping
function, but N64Recomp only reads it for ELF input, not with a symbol
file.)

With that, all 6278 functions recompile with no errors. No `jal` between
overlays was ambiguous; indirect calls into overlays use the runtime's
function lookup, which learns an overlay's functions when the game DMAs it
in (`load_overlays`, called by the runtime's `osPiStartDma`).

### Audio microcode

The audio task setup at `func_80052064` passes ucode text `0x80056F50` and
data `0x80062590`. It's the same `n_aspMain` build as BattleTanx: Global
Assault's: the same 0xC60 size and an identical command dispatch table. So
`n_aspMain.jp.toml` is btgarecomp's config with this ROM's offset
(`0x32350`), and RSPRecomp builds `rsp/n_aspMain.cpp` from it.

### Project setup

Copied from btgarecomp and renamed (`snp_`): the CMake build, the patch
pipeline (`patches/`, `patches.toml`), the frontend (`src/main/`: window,
audio, input, config, crash reporting), the UI assets and the submodules at
the same commits, with btgarecomp's RT64 fixes in `lib-patches/` (Expand
view-projection, matching and interpolation fixes). Left out until this
game needs them: BattleTanx's Controller Pak emulation, multiplayer, and its
game-specific timing, widescreen and scheduler fixes.

- `src/main/main.cpp`: two game entries (Japanese and English), EEPROM
  saves, a Rumble Pak option on the General tab, port 1 only for now.
- `patches/recompui_patches.c`: replaces `func_80046B20`, libsched's
  `__scTaskComplete`, with the same logic plus the frontend's per-frame UI
  pump before the frame swap. It's the game's only per-frame
  `osViSwapBuffer` call.
- `src/game/widescreen.cpp`: the Expand scale from the frontend, for the
  widescreen fixes to come.
- UI theme: the frontend's blue with red and blue swapped (orange).

### Known risks for the first run

- The runtime starts the game by copying 1 MB from ROM `0x1000` to the entry
  point and registering every section in that ROM range
  (`librecomp/src/recomp.cpp`, `load_overlays(0x1000, entrypoint, 1MB)`).
  Here that range also holds seg0 and overlays 1-4, which get registered at
  the wrong addresses (as if loaded right after main). The game's own
  loads register them correctly later, but a later unload that partly
  overlaps one of the wrong entries is fatal ("Cannot partially unload
  section"). If that shows up, limit the startup load to main's size.
- If the game DMAs an overlay in pieces rather than in one go, the runtime
  won't register it (`load_overlays` matches whole sections). The game's
  segment loader would then need a hook.
- The cooperative scheduler issues btgarecomp hit (busy-wait loops waiting on
  another thread) may show up here too; its `scheduler_workaround.cpp` is
  the starting point.
