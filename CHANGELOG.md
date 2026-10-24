# Changelog

## [Unreleased]

### Added
- Spanish voices: tools/make_spanish_voices.py adds the Spanish dub's voices
  to the English translation's ROM, which then loads as the translation.
- Stick Sensitivity on the Controls tab, how fast the stick moves the reticle.
- 8X MSAA, greyed out if your GPU doesn't support it.

### Fixed
- Motion blur in widescreen, broken in 0.2.1.
- Black gaps at the bottom of the screen in widescreen when aiming high
  (from 0.2.1). Only the strips beside the 4:3 picture are cleared now.
- Lock-on didn't work with mouse aim. In lock-on mode, flick the mouse toward
  an enemy to lock on, and flick again to switch targets.
- The reticle drifted off its crosshair lines and stuttered as it appeared at
  framerates above 30. The HUD isn't interpolated anymore.
- Black patches in the corners in widescreen when aiming to the far left or
  right (the bottom corners in stage 1, the top corners in 2-2). The
  backdrop colour reaches the screen's edges now, and anything else past the
  scenery's edge is the colour the far scenery fades into.
- The mouse moved the reticle during the How to Play demo on the title screen,
  so the demo went off script and Saki died. Mouse Aim, Analog Aim and Stick
  Sensitivity are off while the demo plays.
- Jittering objects at framerates above 30, like the flying debris Saki rides
  in stage 2-2.
- Saki snapping into place over and over when strafing at framerates above 30.
- Crates and other parts of the intro's scenery flickering between each other
  at framerates above 30.
- A band along the top and bottom of the screen in widescreen while the
  motion blur is on, not blurred with the rest of the picture.
- The flashing box around the LIFE gauge in the tutorial came apart in
  widescreen.
- Linux: Load ROM could crash on a Wayland desktop (Bazzite) before the file
  picker opened.

## [0.2.1 Beta] - 2026-10-24

### Fixed
- Start Game didn't show after loading a ROM until the app was restarted.
- Flickering, red or smeared bits of background at the sides in widescreen.
  Places with no scenery past the 4:3 view are black now.
- The sky's horizon glow in the Radan fight stopped at the 4:3 edge in
  widescreen.

### Changed
- The translated ROM's Start Game is labeled Translation, not English (Spanish
  and other translations go there too).

## [0.2.0 Beta] - 2026-10-23

### Added
- Mouse aim: Absolute (the reticle follows the pointer) or Relative, with
  sensitivity. Left click fires, right click jumps, middle click switches the
  aim mode.
- Mouse buttons can be rebound, side buttons too.
- Analog Aim option, the stick moves the reticle as far as it's pushed.
- Rice texture packs from the texture_packs folder, with an Open Folder
  button and an on / off option on the Graphics tab.
- Any dump of the Japanese ROM works, and English translations of it, like
  Zoinkity's v1.0.
- Change ROM on the launcher.
- Unlock Everything on the new Enhancements tab.
- Infinite health, time and continues on the new Cheats tab.
- CRT, Scanlines and LCD screen filters, and bloom.
- Add to Steam on the launcher, with artwork.
- F1 opens the settings, as well as Esc.
- crash_log.txt shows more about where a crash happened.

### Changed
- Mouse Aim, Mouse Sensitivity, Joystick Deadzone and Analog Aim are on the
  Controls tab.
- Controls and Mods moved off the launcher, they're in Settings.

### Fixed
- OPT. menu's top row pulled to the edges in widescreen.
- "Fatal error in mod" message when exiting from the launcher.

## [0.1.0] - 2026-10-22

### Added
- Widescreen, HUD anchored to the screen edges and the reticle reaches the
  full width (experimental)
- Framerates above 30 fps (experimental)
- Higher resolutions and anti-aliasing
- Twin-stick controller layout by default, fully remappable
- Supports the original Japanese ROM and the English fan translation
- crash_log.txt generates upon crash
- Linux / Steam Deck build, completely untested, let me know.
