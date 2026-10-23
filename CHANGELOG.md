# Changelog

## [Unreleased]

### Fixed
- Start Game didn't show after loading a ROM until the app was restarted.

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
