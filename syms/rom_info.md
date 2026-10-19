# ROM identification

Two ROMs are supported. They run the same recompiled code.

| | Japan | Fan English translation |
|---|---|---|
| File | `Tsumi to Batsu - Hoshi no Keishousha (Japan).z64` | `Sin and Punishment - Successor of the Earth (U) [!].z64` |
| Internal name | `TSUMI TO BATSU` | `Sin and Punishment` |
| Game code | `NGUJ` | `NGUE` (region byte changed by the patch) |
| Entry point | `0x80025C00` | `0x80025C00` |
| CRC1 / CRC2 | `0xB6BC0FB0` / `0xE3812198` | `0x4F48B0FD` / `0x8E7173F4` |
| SHA-1 | `581297b9d5c3a4c33169ae0aae218c742cd9cbcf` | `5945ff3935d9d9531bbb5cfcabed7c198e2b2f41` |
| XXH3-64 (runtime `rom_hash`) | `0xec641b62243b72b9` | `0x912f22306cafedd2` |
| Size | 32 MiB | 32 MiB |

## The English ROM is a fan translation

Despite the `(U) [!]` name it isn't an official release:

- The first instructions at ROM `0x1000` jump to a stub at `0x1FFFF70`
  (padding in the Japanese ROM) that writes the original instructions back
  and starts the game. Next to it are the strings `N64 PtrTablesV2` and
  `PALadin 3.1`, from ROM patching tools. The recompiled game starts at the
  real entry point, so the stub never runs.
- The text is done by swapping textures. In the asset table at ROM
  `0x34DF4` (groups of three, apparently one per stage) the first file of
  about two dozen groups got smaller; everything after them moved up, about
  155 KB in all. The asset data (`0x7C8680` on) is otherwise the same files.
- The code segments (all of ROM `0x1000`-`0x7C8680`) are byte for byte the
  same, except three instructions that hold asset ROM offsets or a pointer
  that moved with the assets. See STATUS.md round 1.

Both ROMs credit "ENGLISH TRANSLATION bill trinen": the original game's
voices are in English.

## Memory map

`rom = vram - 0x80024C00` for the main segment (IPL3 loads ROM `0x1000` to
the entry point).

| Segment | ROM | VRAM | Text | Notes |
|---|---|---|---|---|
| main | `0x1000` | `0x80025C00` | `0x31280` | game, libultra, libsched, n_audio; then RSP ucode and data. BSS `0x80063450`-`0x800A7070` (cleared by the entry point) |
| seg0 | `0x3E850` | `0x800A7070` | `0x2DE90` | static, right after main's BSS |
| ovl01-23 | `0x6D250`-`0x7757E0` | `0x800E4780` | | overlays sharing one address |
| 24 | `0x7757E0` | `0x802B4000` | none | data only |
| ovl25-27 | `0x777680`-`0x7C8680` | `0x802C3B30` | | small code plus data |

Segments 0-27 come from the game's own load table at ROM `0x34BAC`
(`tools/segments.py`). The Yay0 decoder at `0x80056DD0` is the last function
in main; RSP microcode starts at `0x80056E80` (rspboot), with the audio
microcode at `0x80056F50`.
