# Sin and Punishment (2000 N64) Recompiled and Port

You will need the game file from a licensed copy in order to play it.

A native PC port of Sin and Punishment (Tsumi to Batsu: Hoshi no Keishousha),
made by statically recompiling the N64 game with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) for rendering.

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

**Alpha.** Tested on Windows. The Linux and Steam Deck builds are untested.

## Features

- Widescreen (Expand aspect ratio), with the HUD anchored to the screen edges
  and the reticle reaching the full width (experimental)
- Framerates above 30 fps (Refresh Rate: Display or Manual) (experimental)
- Higher resolutions and anti-aliasing
- Twin-stick controller layout by default, fully remappable, and keyboard
- Enhancements and Cheats tabs: unlock the OPT. menu and every stage in
  Scene Select, infinite health, time and continues
- The Japanese ROM and translation patches of it

## Game Files

You need the Japanese release, Tsumi to Batsu - Hoshi no Keishousha, or a
translation patch of it. Any dump in any byte order (`.z64`, `.n64`, `.v64`)
works, and so does a patched ROM as long as the patch only changes text and
graphics, like the English translations. Tested:

| ROM | SHA-1 |
|---|---|
| Tsumi to Batsu - Hoshi no Keishousha (Japan), original | `581297b9d5c3a4c33169ae0aae218c742cd9cbcf` |
| Sin and Punishment - Successor of the Earth (U), English translation patched onto the Japanese ROM | `5945ff3935d9d9531bbb5cfcabed7c198e2b2f41` |
| Sin and Punishment - Successor of the Earth (English v1.0) [Zoinkity] | `e3906c919236dac6eba020b4b8c1df91fd725c5d` |

The voices are the game's original English voice acting in all of them; the
English translation replaces the Japanese on-screen text and menus. Apply its
patch to the Japanese ROM yourself, as for an emulator.

Add a blank portable.txt in the application's folder to make portable if you
want to save settings locally.

## Steam Deck and Linux

There's a native Linux build (`tools/package-linux.sh` makes the package).
It needs a 64-bit Linux with glibc 2.38 or newer, SDL2, GTK 3, FreeType and
Vulkan, all of which SteamOS has. Not yet tested on a real Steam Deck.

On a Steam Deck:
1. In Desktop Mode, extract the `.tar.gz` somewhere in your home folder.
2. In Steam, add `SnPRecompiled.sh` from that folder as a non-Steam game.
3. Start it from Gaming Mode. Keep Steam Input's default gamepad layout:
   the game's default controls are twin-stick, moving with the left stick
   (the N64 C buttons), aiming with the right stick, firing with the right
   trigger.

Settings and saves are in `~/.config/snprecomp` (or next to the game with
`portable.txt`). The Windows build also runs under Proton.

## Issues and Limitations

Known:
- Widescreen and framerates above 30 fps are experimental: if something
  looks wrong, check it at Original aspect ratio and Original refresh rate,
  and please report it with a screenshot or clip.
- A few scenes show sky or nothing at the sides in widescreen, where the game
  has no scenery past the original 4:3 view.
- The second-long zoom into the title screen stays 4:3.

Else:
- Please see the "Issues" section in GitHub.

## Building

See [BUILDING.md](BUILDING.md).

## Credits

- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- Zoinkity, for the English translation of the game's text
- [spimdisasm and rabbitizer](https://github.com/Decompollaborate) for finding
  function boundaries, and [n64sym](https://github.com/shygoo/n64sym) by shygoo
  for identifying the N64 SDK's functions
- The app icon's 罪 is drawn with [Dela Gothic One](https://github.com/syakuzen/DelaGothic)
  (SIL Open Font License 1.1)

## License

The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself remains
the property of its rights holders.
