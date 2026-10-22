"""Prints the code fingerprint src/game/rom_version.cpp checks ROMs against.

ROMs whose fingerprint matches the Japanese release's run this port, whatever
else a patch changed. Usage: rom_fingerprint.py <rom.z64> [...] (big-endian).
"""
import struct
import sys

MAIN_VRAM = 0x80025C00
MAIN_ROM = 0x1000
MAIN_TEXT_SIZE = 0x31280
LOAD_TABLE_ROM = 0x34BAC
# Words translations change (rom_version.cpp's varying_words).
VARYING_WORDS = [0x80025C00, 0x80025C04, 0x80025C08, 0x80025C0C,
                 0x8002BD5C, 0x8002BD8C, 0x8002BD98,
                 0x8003C158, 0x8003C15C, 0x8003C198, 0x8003C19C, 0x8003C1EC, 0x8003C1F0]


def fnv1a(data, h):
    for b in data:
        h = ((h ^ b) * 0x100000001b3) & 0xFFFFFFFFFFFFFFFF
    return h


def fingerprint(rom):
    main = bytearray(rom[MAIN_ROM:MAIN_ROM + MAIN_TEXT_SIZE])
    for vram in VARYING_WORDS:
        o = vram - MAIN_VRAM
        main[o:o + 4] = bytes(4)
    h = fnv1a(main, 0xcbf29ce484222325)
    p = LOAD_TABLE_ROM
    while True:
        entry = rom[p:p + 20]
        rom_start, vram, text_end, _, _ = struct.unpack(">5I", entry)
        h = fnv1a(entry, h)
        if rom_start == 0 or vram == 0:
            return h
        h = fnv1a(rom[rom_start:rom_start + text_end - vram], h)
        p += 20


if __name__ == "__main__":
    for path in sys.argv[1:]:
        with open(path, "rb") as f:
            print("%016x  %s" % (fingerprint(f.read()), path))
