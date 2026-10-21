# Sin and Punishment (2000 N64) Recompiled and Port

You will need the game file from a licensed copy in order to play it.

A native PC port of Sin and Punishment (Tsumi to Batsu: Hoshi no Keishousha),
made by statically recompiling the N64 game with
[N64Recomp](https://github.com/N64Recomp/N64Recomp) and running it on
[N64ModernRuntime](https://github.com/N64Recomp/N64ModernRuntime) with
[RT64](https://github.com/rt64/rt64) for rendering.

Unofficial, and not affiliated with the rights holders. **No game data is
included.** You need your own dump of the game to build or play this.

**Status: early work in progress.** Both ROMs boot, and the menus and the
first stage play; the rest isn't tested yet. See [PROGRESS.md](PROGRESS.md).

## Game Files

Either of these ROMs works. Pick the one you want to play:

| ROM | SHA-1 |
|---|---|
| Tsumi to Batsu - Hoshi no Keishousha (Japan) | `581297b9d5c3a4c33169ae0aae218c742cd9cbcf` |
| Sin and Punishment, fan English translation | `5945ff3935d9d9531bbb5cfcabed7c198e2b2f41` |

The voices are the game's original English voice acting in both. The fan
translation replaces the Japanese on-screen text and menus with English;
apply its patch to the Japanese ROM yourself, as for an emulator. Hashes are
of the big-endian `.z64`; `tools/normalize_rom.py` converts other dumps and
says whether yours is one of these.

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

## Planned

- Higher framerates (interpolated) and widescreen, with the HUD anchored to
  the screen edges and the aiming crosshair matched to the wider view.
- For texture pack and mod makers: texture packs loaded from the mods folder,
  a texture dump switch, a list of the text textures that differ between the
  two ROMs, and the function names found while porting, shared by both ROMs.

## Building

See [BUILDING.md](BUILDING.md).

## Credits

- [N64Recomp and N64ModernRuntime](https://github.com/N64Recomp) by Mr-Wiseguy
  and contributors, which this port is built with
- [RT64](https://github.com/rt64/rt64) by Dario and contributors, the renderer
- [RecompFrontend](https://github.com/N64Recomp/RecompFrontend) by the N64Recomp
  contributors, the launcher and input layer
- [spimdisasm and rabbitizer](https://github.com/Decompollaborate) for finding
  function boundaries, and [n64sym](https://github.com/shygoo/n64sym) by shygoo
  for identifying the N64 SDK's functions

## License

The project's own code is GPL-3.0; see [COPYING](COPYING). The game itself remains
the property of its rights holders.
