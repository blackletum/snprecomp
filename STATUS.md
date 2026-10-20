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

## Round 2: first boots

Run headless for testing: Xvfb with Mesa's software Vulkan (lavapipe), the
ROMs stored in a portable config folder as `<game id>.z64`, started with
`--game snp`. (`--game` matches the mod game id, which both ROMs share, so
it starts whichever registered entry comes first; both ROMs are valid
here.) Thread stacks come from gdb attached to the running game.

### Link: SDK functions n64sym missed

Four SDK functions without an n64sym match were recompiled from the ROM,
and their calls into functions N64Recomp replaces didn't link. Named from
what they call (`EXTRA_NAMES` in `tools/gen_syms_toml.py`) so N64Recomp
replaces or skips them too: `osEPiStartDma` (`0x8003EBB0`), `osContInit`
(`0x800433B0`), `viMgrMain` (`0x80048110`), `__osRepairPackId`
(`0x8004ABF8`).

### Missing functions

- `0x80025C40`: the entry stub ends in `jr $t2` to it, and spimdisasm had
  merged the two. Split by an override.
- `0x8004E640`, a thread entry point only reached through a pointer.
  `tools/gen_syms_toml.py` now starts a function at every address the code
  builds with lui/addiu or stores in a segment's data, when it begins a
  stack frame and the code before it never falls through. The first
  version, without the stack-frame rule, split jump tables (their case
  labels are code addresses in data too) and addresses made up by a `lui`
  and an unrelated `addiu`; N64Recomp then failed on the switch statements.
  That adds 8 functions.

### Busy-wait loops

With every function present, the game started its threads and then
stalled: the main thread waited for a retrace message that never came.
The idle thread (`func_80025CA4`, priority 0) ends in a loop polling
`D_800657B0` for a callback, with no OS call. On the N64 an interrupt
preempts it; the runtime only switches threads inside OS calls, and only
delivers VI and other events when a game thread waits for one, so the
spinning idle thread kept everything else waiting.

Two more loops do the same: the main thread polls `D_80068A95`
(`func_80025E44`) and `func_8004D7B0` waits for the job thread
(`func_8004D7C8`) to finish its jobs (`D_8007AB94` counts them).
`tools/find_spin_loops.py` lists short loops that only load a fixed
address and branch; besides these three it finds only hardware register
polls inside SDK functions the runtime replaces.

Each loop head gets a hook calling `snp_spin_yield`
(`src/game/spin_wait.cpp`): wait up to 1 ms for an event, deliver every
pending one, switch to any higher-priority thread that's ready, then drop
the thread to priority 0 and back so lower-priority ready threads (the job
thread, for the main thread) get to run too. The priority drop does nothing
for the idle thread, already at 0: `osSetThreadPri` returns early when the
priority doesn't change, so the explicit switch is needed (the first
version without it stalled again at the same wait).

### Segment loading

The game's segment loader `func_8003A5F0` (called by `func_8003A1D0` with
a load table entry) copies a segment in 0x1000-byte `osPiStartDma`
chunks. The runtime registers a code section only when one DMA covers all
of it, so most overlays would never be registered. A hook after the copy
loop (`src/game/overlay_loader.cpp`) registers the segment once it's in
place, after unloading every section in the span any overlay at that
address covers (overlays sharing an address differ in size; a partial
unload is fatal). On the first load it also drops the sections the
runtime's startup registered at wrong addresses (round 1's first risk).

`osAiGetLength` (`0x8003F350`, between `__osAiDeviceBusy` and
`osAiGetStatus`) was the next unnamed SDK function: recompiled, it read the
AI length register and crashed when audio started. Named too. A scan for
recompiled functions that build hardware register addresses turns up only
`func_8004BA30` (PI registers, only called from `__osInitialize_common`,
which the runtime replaces) and float constants (`lui 0xBF80` is -1.0f).

### Result: the attract mode runs

With these, both ROMs boot into the attract mode: the intro cutscenes in
3D with "PRESS START BUTTON" and subtitles, English with the fan
translation and Japanese with the Japanese ROM. Not checked yet: sound (no
audio device headless), input, the title menu and gameplay.

Some cutscene shots already draw wider than 4:3 on their own (the side
areas in a few shots); to look at with widescreen.

### `--game`

Both ROMs share the mod game id `snp`, so mods apply to either. The
runtime's `--game` option matched only the mod game id and stopped at the
first match, even one without a ROM. A runtime patch
(`lib-patches/N64ModernRuntime/`, applied at configure time like the RT64
fixes) makes it take the first match with a valid ROM and accept a full
game id too: `--game snp.n64.jp.1.0` or `--game snp.n64.en.fan`.

## Round 3: menus and the first stage

Input works headless too (`xdotool` keys into the Xvfb window; the
frontend's keyboard defaults: Return is Start, WASD the stick, Q is Z).
Start on the attract mode gives the title screen and the main menu, which
is driven with the aiming cursor (stick to move, Z to select).

### Division by zero

Selecting START GAME crashed with a host floating point exception in
`func_800A9954`: an unsigned divide by zero (`divu` at `0x800A99B0`,
`0x8000 / x`). The game was compiled without zero-division checks: of its
1111 `div`/`divu`, only 9 are followed by a `break`. On the VR4300 a
division by zero doesn't trap: the quotient is -1 (1 for a negative signed
dividend) and the remainder is the dividend. N64Recomp emitted a plain C
division, which traps on the host.

btgarecomp guarded the few divisions that hit this with per-site hooks;
with 1111 here, N64Recomp itself is fixed instead
(`lib-patches/N64Recomp/`): `div`/`divu` are emitted as `DIV32`/`DIVU32`,
and `DDIV`/`DDIVU` handle zero too, all giving the VR4300's results. Since
it changes the generated code, `RecompiledFuncs/` now has to be generated
with the N64Recomp the main build makes (with the patch applied), and the
build regenerates it when an N64Recomp patch changes. BUILDING.md's steps
changed accordingly.

### Result: the first stage plays

START GAME now runs stage 1: the player, enemies, the HUD (score, time,
life) and the aiming crosshair; shooting scores. The crosshair's two red
guide lines reach past the 4:3 frame into the window's side areas (to look
at with widescreen).

## Round 4: the launcher takes either ROM

First Windows test: the English ROM was refused with "This ROM is not the
correct game". The frontend's default launcher only offers the first
registered game (`supported_games[0]`, the Japanese ROM) and checks a
loaded ROM against that entry's hash alone; the English entry was never
offered. `src/main/launcher.cpp` replaces it: Load ROM tries the file
against each supported game and stores it under the one it matches, and
there's a start option per loaded ROM ("Start Game" with one, "Start Game
(English)" / "Start Game (Japanese)" with both; Load Another ROM goes away
once both are loaded). Controls, Settings, Mods and Exit are the
frontend's own.

Still to check: with the keyboard, the highlight starts on Controls rather
than the start option.

## Round 5: widescreen (Expand) part 1; the real frame swap; green theme

Windows reports: in Expand, old frames and bright red stripes showed beside
the picture in cutscenes, the title screen and gameplay; the crosshair has
thin lines across the screen; models jitter in cutscenes (interpolation,
see Next).

### What the game draws

A display list diagnostic (`src/game/display_list.cpp`, `SNP_DL_DIAG=1`)
walks each frame's graphics task and logs color images, scissors,
viewports, fill and texture rectangles and render modes. It's hooked right
before the game's task threads (`func_8004E640`, `func_8004E4A0`) call
`osSpTaskLoad`, with the `OSTask` in `$a0`.

- Every frame is drawn inside an inset: scissor 12,8 - 308,231, cleared
  only there (fill 12,8 - 307,231). At the end the game opens the scissor
  to the full screen and draws a black border around the inset (four fill
  rectangles). The viewport is the full screen.
- RT64 widens a 3D view in Expand only when its scissor spans the full
  screen width, so the picture stayed 4:3, and the side areas, which
  nothing clears, kept old frames.
- The crosshair's lines are the game's own: full-width and full-height fill
  rectangles through the crosshair, built like its HUD boxes (outline and
  fill), drawn translucent (1-cycle, alpha blend, primitive alpha) and
  clipped to the inset. They're faint in Original aspect, at native
  resolution too, so they're likely on hardware as well (not yet compared).
  In Expand RT64 extends full-width rectangles past the scissor, and with
  nothing clearing the side areas each frame's translucent line blended
  over the last into bright stripes.

### Fix (Expand only)

`snp_display_list_hook` rewrites the frame's display list before it's
loaded, when the frontend's widescreen scale is above 1 (Expand with a
wider window):
- the inset scissor becomes the full screen, so RT64 widens the view;
- fill rectangles spanning the inset's width (x 12 - 307: the clear, fades,
  tints, the backdrop band behind the grass in stage 1) span the full
  width, and the full screen height too when they span the inset's height
  (y 8 - 231), or the rows above and below the inset keep old frames;
- the border's four rectangles become no-ops.

Result: cutscenes and stage 1 fill the window in Expand, with nothing stale
beside them. Still 4:3 in Expand: the game's framebuffer effects, which
redraw the previous frame as a texture over the inset (scene cross-fades,
the motion-blurred zoom behind the title logo, some tinted shots). The HUD
isn't anchored to the edges yet.

### The frame swap and the UI pump

The diagnostic was first called from the round 1 patch of libsched's
`__scTaskComplete` and never ran: the game links libsched but runs its own
scheduler. Its frame swap is `func_8004D748`, a job for the job thread
(`func_8004D7C8`) that calls `osViSwapBuffer` with the job's framebuffer
(offset 0x0C). The UI pump patch (`patches/recompui_patches.c`) now
replaces that function instead, so the frontend's per-frame UI callbacks
actually run in game.

### Theme

Green instead of orange, after the title logo's black, green and white
(the frontend's blue palette with green and blue swapped).

## Round 6: audio crackle; quitting on Windows

### Constant crackle

`SNP_AUDIO_DIAG=1` (in `queue_samples`, `src/main/main.cpp`) logs per
second the audio the game delivered, chunk sizes, the queue's range and
drops. Run headless with SDL's real-time `disk` audio driver
(`SDL_AUDIODRIVER=disk`), it showed the game delivering about 28,500 frames
a second for a 22,050 Hz output: always 920-frame chunks, 30 a second. The
queue sat at the 200 ms latency cap and about 6 chunks a second were
dropped, each an audible skip.

The game's audio thread (`func_80052064`, libmus) sizes each frame from
`osAiGetLength` with hysteresis: frame 920 (184-sample steps, the
`func_800521DC` sizing), 736 or 1104 to correct, but a correction only
after the length was in its target band (220 - 404 samples) the frame
before (`func_80052268`, flag `D_800622B0`). And it skips a frame while
`osAiGetStatus` says the AI FIFO is full. On hardware `AI_LEN` is what's
left of the buffer playing now (0 - 920 samples) and the FIFO holds two
buffers. The runtime reports the whole queue as the length and never full,
so the length jumped past the band, no correction ever started, and the
game wrote 25% too much. (Not btgarecomp's under-reported "headroom", which
this game doesn't use: removing it changed nothing.)

Fix: two hooks in `func_80052064` replace the results of its
`osAiGetStatus` and `osAiGetLength` calls with hardware-like ones from the
frontend, which tracks the chunks it queued: the length is what's left of
the chunk playing now, and the FIFO is full while two chunks wait behind it
(one more than hardware, for host thread wakeup jitter). Measured: 22,050
frames a second on average in 736/920/1104 chunks, queue 32 - 90 ms, no
drops, never empty after startup.

(The game's `__osInitialize_common` does the `osClockRate * 3 / 4` step
that btgarecomp round 138 had to redo, but nothing else in this game reads
`osClockRate`, so nothing runs slow here.)

### Quitting on Windows: "stopped interacting with Windows"

Quitting from the in-game menu ended in a Windows cross-thread hang report.
First seen on a build from before round 5, whose UI pump never ran; still
there after it. Quitting exits cleanly here on Linux. On quit, the runtime
joins the game, graphics and event threads before `recomp::start` returns;
when that takes a while the window stops pumping messages and Windows
closes the program as hung. A watchdog in `main.cpp` ends the process 3
seconds after quitting starts (the runtime's `exited` flag) if it's still
running. Saves are written as the game goes, so nothing needs the orderly
shutdown.
