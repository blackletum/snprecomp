# Android port: plan

Status: steps 1 and 2 done: the game builds from Android Studio and plays well on the owner's
Retroid Pocket 6 (first run, 2026-10-25). Next: the checks under "Watch" (step 3), then touch.
Written 2026-10-25 so a new session can start without re-deciding anything.

## Decisions (agreed with the project owner)

- **Same approach as the owner's BattleTanx: Global Assault port** (wootbeer/btgarecomp,
  branch `claude/modest-ramanujan-s0wyga`, whose `android/PLAN.md` has the full history of
  every Android problem met there). That code is the owner's and is reused here as is
  where it fits: `android/`, `cmake/Android.cmake`, `cmake/CrossCompile.cmake`,
  `cmake/android/`, `src/android/`, and its Android lib-patches.
- **Branch `android`** of this repo; `main`'s desktop build stays unaffected (all Android
  code is behind `ANDROID` / `__ANDROID__` or `CMAKE_CROSSCOMPILING`). Merge `main` into
  `android` from time to time.
- **Ship the compiled game code in the APK**, like the Windows and Linux releases. The user
  supplies their own ROM; the APK contains no ROM data (the build checks).
- **Android Studio workflow**: a second local checkout on this branch; open its `android/`
  folder, press Run (android/README.md).
- **First milestone: controller-only**, booting on the owner's Retroid Pocket 6
  (Snapdragon 8 Gen 2, Adreno 740). Touch controls come after (see Touch controls below).

## Toolchain (same as BTGA)

Android Gradle Plugin 9.3.2, Gradle wrapper 9.5.0, NDK r28c (28.2.13676358), SDK CMake
3.31.6 (not 4.x), `compileSdk`/`targetSdk` 35, `minSdk` 28 (lib/SlotMap's `aligned_alloc`),
`arm64-v8a` only, `ANDROID_STL=c++_shared`, debug builds `RelWithDebInfo`. SDL 2.32.10 and
FreeType 2.13.3 built from source (SHA-256 pinned). App id `io.github.wootbeer.snprecomp`.

## What came over from BTGA

Submodules: RT64, N64ModernRuntime and RecompFrontend are at the same commits as in BTGA, so
its patches apply unchanged. Renumbered so they sort after this repo's own:

| BTGA | here | what |
| --- | --- | --- |
| N64Recomp 0001 | N64Recomp 0002 | float-to-int conversions give x86's results on ARM64 |
| plume 0001 | plume 0001 | Adreno: boundless descriptor pool sized to the layout |
| plume 0002 | plume 0002 | new surface after the app was in the background |
| plume 0003, 0004 | (already here) | Vulkan scissors / clear rects clipped |
| plume 0005 | plume 0005 | Adreno: read-only depth loaded, not LOAD_OP_NONE |
| rt64 0002 | rt64 0002 | DXC by host; file_to_c / nfd from the parent project |
| rt64 0003 | rt64 0003 | application window without X11 |
| rt64 0004 | rt64 0004 | RT64's data folder in app storage |
| rt64 0005 | rt64 0005 | RGBA8 swap chain |
| rt64 0006 | rt64 0006 | no framebuffers for an invalid swap chain (all platforms) |
| rt64 0008 | rt64 0008 | dithered alpha compare drawn with the ubershader (Adreno grain) |
| rt64 0016 | rt64 0022 | shader descriptions kept between runs (no first-use stutter) |
| RecompFrontend 0001-0005 | RecompFrontend 0013-0017 | native window, app folder, swap chain format, Window Mode hidden, no Open Mods Folder |

Not taken: rt64 0009 (BTGA's own matrix-ID pairing), 0017 (AMD RDNA1 PC workaround), 0021
(BTGA's auto-match group limit), RecompFrontend 0011-0013 (BTGA menu changes). rt64
0011-0015 and 0018-0019 were already here (0011 differs slightly from BTGA's; this repo's
is kept).

Native code: `src/android/android_startup.cpp` (stdout/stderr to logcat, tag `SnP`; the
surface provider for plume; working directory = app storage; input devices logged) and
`src/android/nfd_android.cpp` (the file dialogs through the system document picker).
`cmake/Android.cmake` also turns FMA contraction off (BTGA found it changes the maths).

Java: `SnPActivity` (BTGA's activity without the touch overlay): copies `assets/` into app
storage, keeps the system bars hidden, opens the document picker for the game, and the
system Back opens the game's menu.

Differences from BTGA:

- **No separate ROM picker activity.** This game's launcher (`src/main/launcher.cpp`) already
  has Load ROM / Change ROM for both supported ROMs (Japanese and the English translation),
  through recompui's file dialog, which on Android is the document picker. BTGA's in-game
  Change ROM works the same way on the device, after its surface fixes. Picked copies are
  deleted at the next start.
- Root `CMakeLists.txt`: the same changes as BTGA (cross-compile includes, no regeneration
  when cross-compiling, patches `make` on `CMAKE_HOST_WIN32`, DXC by host, `libmain.so`,
  SDL / FreeType / include folders).
- `src/main/main.cpp`: Vulkan + fullscreen window on Android, `snp::android::startup()`.

## SnP-specific features on Android

| feature | state |
| --- | --- |
| Add to Steam (launcher) | hidden on Android |
| Texture Packs, Open Folder | button hidden (app storage can't be opened by a file manager) |
| Rice texture packs (RT64 0020) | compiled in; no way to put a pack in `texture_packs/` yet: later, e.g. an Install Pack button taking a .zip through the picker |
| RT64 packs (.rtz, Mods tab, Install Mods) | the picker allows several files, as in BTGA; untested |
| Load Spanish ROM (Sound tab) | the picker opens (owner); loading a dub and hearing it still to test |
| Screen Filter, Bloom (RT64 0018, 0019) | same patches as BTGA, which work there; to test |
| MSAA 8x (RecompFrontend 0010) | to test |
| Mouse aiming | inert without a mouse; replaced by touch aiming later |
| Pacing log (RT64 0017) | logs to logcat; useful for the frame-pacing checks below |

## Watch: frame pacing, interpolation, widescreen

BTGA met Android-only versions of several desktop bugs (jitter at Display refresh rate,
HUD lost in Expand, grain, flashing). The general fixes are in (above); the game-specific
ones there were its own code. For this game, compare the Retroid with the PC on:

- Original vs Display refresh rate (120 Hz on the Retroid): camera and reticle smoothness,
  enemies and bullets, any jitter or flashing (this repo's rt64 0011-0016 interpolation
  patches; BTGA needed FMA off for its matrix decomposition, already done here).
- 4:3 vs Expand: HUD, the reticle clamp (`src/game/widescreen.cpp`), letterbox and
  full-screen fades or flashes (BTGA's nuke flash covered the black sides).
- Resolution Auto vs Original, MSAA, Screen Filter, Bloom.
- Smoke, explosions and see-through effects (Adreno grain, BTGA's depth fix).
- Lag spikes the first time effects appear (shader descriptions kept from run 2 on).
- Background / lock screen and back.

## Order of work

1. Port BTGA's Android work (done, see Progress).
2. Owner: build from the Android checkout, run on the Retroid, send `adb logcat -s SnP`
   (and the crash report if it crashes).
3. Fix what comes up; the checks above.
4. Pause emulation while the app is in the background (still to do in BTGA too).
5. Touch controls (done, to test; see Touch controls).
6. Release: signing (keystore.properties), versionCode/versionName following the PC release.

## Touch controls

Owner's direction: **touch takes the place of the mouse aiming** this repo added for PC
(`src/main/mouse.cpp`), it works **together with a gamepad** (on the Retroid the sticks and
triggers play while the screen aims), and there is a **touch-to-shoot** mode, separate from
the on-screen buttons phones need.

Done (step 3, to test on the device), settings on a Touch tab (Android only, after
Controls; `src/main/game_config.cpp`):

- **Touch Aim**: Off / **Point** (default: the reticle goes where the finger is, like the
  mouse's Absolute) / **Drag** (trackpad-like, by **Touch Aim Speed**, 100% = with the
  finger). The finger goes through the same hooks as the mouse (`snp_mouse_reticle`,
  `snp_aim_stick`): it wins over the mouse in a frame both moved; locked on, the game
  steers and a swipe flicks to the next target, as a mouse flick does.
- **Fire While Touching** (default on): Z held while the aiming finger is down. With Point
  that's touch-to-shoot.
- **On-screen Buttons**: Auto (default: shown unless a gamepad is connected, a handheld's
  built-in one included) / Always / Never; **Button Size**, **Button Opacity**. Layout
  (`src/android/touch_controls.cpp`): movement stick bottom left (the C buttons, the way the
  game moves; past half way holds that direction, so two quick pushes roll), FIRE (Z)
  bottom right with JUMP (R) beside it and AIM (A, the aim mode) above it, MENU and START top
  centre.
- Routing: while the game takes input `TouchControlsView` takes every touch and native code
  gives each finger to a button (descore, `src/android/descore/touch/`, from BTGA unchanged)
  or, if it lands on none, to aiming (the first such finger). So one finger aims while others
  move, jump or fire, and SDL never sees in-game touches (it would make them mouse clicks,
  and left click is bound to fire). Under the game's menus and on the launcher touches go to
  SDL as before. A menu opening under a finger lets go of everything.
- Unlike BTGA's, the buttons don't hide on gamepad input and come back on a touch: that
  would fight touch aiming. They follow whether a gamepad is connected.

Owner: touch works on the Retroid. The tab is named just "Touch" (owner: one line in the
tab bar); its id stays `touch_controls`, so saved settings carry over.

To check on the device: Point and Drag feel, Fire While Touching, the buttons on a phone
(or with On-screen Buttons on Always), whether the stick's up/down (C up/down) does anything
unwanted in this game, two fingers at once, the menu opening mid-touch.

## Progress

### Step 1 (2026-10-25) -- done in the sandbox

- Copied and renamed BTGA's Android files (above); `.gitignore` has `build-*/`.
- Checked: every lib patch applies in order on fresh submodules.
- Sandbox build with stand-in (empty) generated sources, NDK r28c: everything compiles; the
  only link errors of `libmain.so` are the game's own symbols the stand-ins lack
  (`recomp_entrypoint`, `n_aspMain`). With the native targets cut to `SDL2`,
  `gradlew assembleDebug` packages the APK (`SnPRecompiled-0.2.3-android-debug.apk`,
  `io.github.wootbeer.snprecomp`, launcher activity `SnPActivity`, assets and licenses in it).
- `cmake/ApplyLibPatches.cmake`: re-applied rt64's and RecompFrontend's patches on every
  configure (also on main), because a patch that a later one changes again (rt64 0018,
  RecompFrontend 0002 and 0006) no longer reverse-checks on its own; every Gradle sync then
  rebuilt both. The stamp now records the patched working tree's Git tree ID and compares
  that instead. Checked: unchanged trees are left alone; a file a patch changed or added,
  undone by hand, gets that submodule's patches applied again.

### Step 2 (2026-10-25) -- first device run

Owner built from the second checkout (PowerShell setup as in android/README.md) and ran it
from Android Studio on the Retroid Pocket 6: boots, ROM loads through the launcher, and it
"plays really well thus far" with the controller. Both ROMs (Japanese and the English
translation) loaded side by side through Load ROM, with a Start Game for each; played the
translation. Load Spanish ROM opens the picker. Not yet gone through: the checks under
"Watch" and the SnP-specific features table.

### Cones trading places (city stage, Display rate)

Owner: the city stage's cones trade places on Android at Display rate (60, 90, 120 Hz), fine
at Original; the PC is fine at every rate (STATUS.md, "Identical objects flickering": the fix
numbers alike objects by matching them to last frame's, closest pairs first). Not threading
(the hooks run in the game's task threads, one at a time). The numbering is plain float maths,
the same on ARM, except one step: candidate pairs were sorted with `std::sort`, which leaves
equal distances in whatever order the library likes, and libc++ (Android) orders them
differently from the desktop libraries (checked here). Now `std::stable_sort` (all
platforms). To check on the device.

If not that: `adb shell setprop debug.snp.interplog 1`, start the game, play the cones for
~20 s, quit, then `adb shell run-as io.github.wootbeer.snprecomp cat files/snp_interp.log >
snp_interp.log`. Per display list: how old the instances drawn are (a different build/walk
timing on Android would show there), and each object kind's numbering (positions, matches,
distances). `SNP_INTERP_LOG=1` does the same on desktop. (A flag file works too, for when
the property can't be read: `adb shell run-as io.github.wootbeer.snprecomp touch
files/snp-interp-log`, force-stop, start.)

Owner: the stable sort didn't change it ("still flash and jiggle and seem to swap"). The log
(~450 display lists in the cone scene): numbering is right. Static objects match themselves
at distance 0 every list, alike ones keep their numbers when their drawing order changes,
nothing drops out as stale (instances drawn were built 1 or 2 lists earlier, never 3), and
the counts drawn are steady. So the IDs RT64 gets are stable; the swapping is in RT64's
interpolation itself. Its code is the same as on PC except for hlslpp, its vector maths, which
on ARM uses NEON with division and square root from estimates plus Newton steps (SSE: exact).
Interpolation decomposes matrices (scale from square roots, rotation), so a slightly
different result can tip a borderline choice. Test: `HLSLPP_SCALAR` on Android
(`cmake/Android.cmake`).

Owner: with scalar hlslpp the cones were fine (a flicker as the stage first loads, then
steady), but after quitting from the game's menu and starting the level again they traded
places as badly as before. So not hlslpp (back to NEON): it depends on the run. The first
log only counted matrices whose record passed the age check (built at most 2 display lists
before the walk; older ones are left to RT64's automatic matching, which is what swaps
alike cones), so a run whose timing goes past that wouldn't have shown. The log now gives,
per display list, the lookups by outcome (found 0/1/2 lists ago, stale 3/4/5/6+, never
recorded) and matrix loads past the tag area. Next: a log of the bad case (quit, restart the
level), and the same on PC (SNP_INTERP_LOG=1) to compare.
Earlier note, kept for the record: (Correction to the above: on aarch64 hlslpp's division and square
root are the native exact instructions, and its multiply-adds aren't fused with
`-ffp-contract=off`; its NEON reciprocal and some other operations still differ from SSE's.
Which one tipped RT64 wasn't narrowed down: each try needs a device run.) The cost is RT64's
CPU-side vector maths without SIMD; watch busy scenes. Still to see: whether the flicker as
the stage loads also happens on PC (likely the first frames, before there is a last frame to
match, or a shader compiled on first use).

Owner: the PC build does it too, after quitting and starting the level again. So not Android:
a bug in the cone fix (main's too). Likely cause, fixed (to test): every matrix load is
replaced with a call into a tag entry (4 banks, one per graphics task, reused in turn) that
holds its ID and the game's load. A list the game keeps (scenery) still holds that call the
next time it's walked, and the walk restored the game's load by reading it back from the
entry. A kept list not drawn for 4 or more tasks (the stage while the game is left and
started again) calls an entry since filled with another load, so alike models were restored
with each other's matrices. `src/game/display_list.cpp` now keeps each tagged command's
original load by address and restores from that. Port to main once confirmed.

Owner: still swapping with that, and the bad case can come on the first play too (the good
first runs were luck); fine at Original (so drawing data is right, only pairing on in-between
frames). The runtime hands RT64 one graphics task at a time, read before the next, so the
tag banks can't be overwritten in use; the scratch areas are in RAM the game never uses. The
restore record stays (it's right either way). The logs show the actual cause: the positions
objects are numbered by are their own matrices' translations, local to the parent they're
drawn under, and alike objects under different parents sit at exactly the same local place
(kind E8413EC7 twice at (0 0 900), 91629509 twice at (0 0 180)). Ties go by drawing order,
so when that changed the two traded numbers, and RT64 blended each between the two places on
in-between frames (the log can't show it: numbers are handed out in drawing order). The
display list walk now keeps the RSP's modelview matrix stack (G_MTX load / mul / push,
G_POPMTX), records each object's combined place, and exact ties are broken by that. Other
matching is unchanged. The log prints the places too.

Owner: still there; it's the cones shifting left and right with ghosting, also with the camera
still, nothing else. The places in the log equal the local positions (these objects are
loaded, not multiplied onto a parent; the camera is in the projection), so that tie-break
did nothing here (kept: harmless). The log shows the real cause: the four cones stand in four
spots (x -75, -25, 25, 75 at z -450), drawn with two models (kinds 8043C575 and E42124E1, two
frames of the cone, it seems), and a spot changes model from frame to frame. A model's object
in one spot was then paired with its own model's object in another spot, 50-150 units away,
and RT64 slid it across on in-between frames, back and forth. Now an object exactly (within
1 unit) where another kind's object was last time, and no object of its own kind, gets a new
number: drawn where it is. (`last_object_places`, `src/game/interpolation.cpp`.)

Owner: mostly fixed; the cones still flash while they fly (walking into them kicks them into
the air, as in the game). They keep switching models in the air, where no spot is shared, so
a flying cone was paired with another of its model or with nothing. Now kinds are joined into
a family once objects of each have taken over the other's exact spot 3 times (`family`,
`note_takeover`): a family is numbered and given IDs as one kind, so a cone keeps its number
with either model, standing or flying. Two kinds both drawn at a spot every frame never count.

### Merge of main 0.2.4 and after (2026-10-26)

Brought in: Music and Sound Effects Volume, the intro warehouse's side walls in widescreen,
Texture Filtering and HUD Filtering, Screen Filter: Menus Only, and Smooth Aiming (RT64 0021,
`live-offset`). This branch's shader descriptions patch moved to rt64 0022; all patches apply
in order. Smooth Aiming's `live_offset` (`src/main/mouse.cpp`) takes a finger as it takes the
mouse: Point like Absolute, Drag like Relative, a finger first. Version 0.2.4 (versionCode 2).
The TOML has new hooks: regenerate the recompiled code with the desktop build and copy
RecompiledFuncs/, RecompiledPatches/ and rsp/ again. Still open after the release: the
screen filter on the HUD and crosshair only.

Owner: 0.2.4 on the Retroid tested good (regenerated game code, Android Studio Run).

### Crash after the Nintendo logo on Adreno 619 (2026-10-26)

Reports: GitHub issue 14 (Adreno 619) and the owner's own phone (a Motorola, Adreno 619,
Vulkan driver 512.530); a Reddit report from an S24 Ultra (Adreno 750, Android 16, black
screen then closed before the launcher) has no log yet and may be something else. The owner's
logcat: all 8 of RT64's ubershader pipelines (`RasterShaderUber`, one per zCmp/zUpd/cvgAdd)
fail in the driver ("AdrenoVK: Failed to link shaders", `vkCreateGraphicsPipelines` returns
VK_ERROR_UNKNOWN); plume leaves the pipeline's handle null, and the first draw that falls back
to the ubershader binds it: segfault in `vkCmdBindPipeline` (read of 0x128).

Fix (all platforms; nothing changes where the ubershader compiles): plume 0006 gives pipelines
`isValid()` and skips binding one that isn't; rt64 0023 has `RasterShaderUber::getPipeline`
return null for a failed pipeline, logs how many failed, and a draw then uses its own
(specialized) shader if that's compiled and valid, else is skipped until it is. Patch 0008's
dithered-alpha draws fall back to their own shader too. Expect objects to pop in on a first
run while their shaders compile; later runs compile the shaders seen before at startup
(rt64 0022). To test on the owner's phone: no crash; whether the specialized shaders link on
this driver (no "Failed to link shaders" after the 8 ubershader ones); how long pop-in lasts.

Also: the launcher opened in portrait on that phone. With a resizable window and no
orientation hint, SDL requests FULL_USER, overriding the manifest's sensorLandscape;
`snp::android::startup()` now sets `SDL_HINT_ORIENTATIONS` to both landscapes.

Owner: works on the Motorola now (no crash, plays).

