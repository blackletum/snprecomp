"""Makes a ROM with the English translation's text and the Spanish dub's voices.

The Spanish translation (IlDucci, PacoChan, Zoinkity and others, 2015) changes
the game's code to show its text, so this port can't run it. Its dub's voices
are only sound data: they're added to the English translation's ROM past its
end, the four instructions that give the sound data's ROM offset are pointed
at them (the port reads them from the loaded ROM), and so is the table of the
voice banks' ROM offsets in the main program's data.

Usage: make_spanish_voices.py <english.z64> <spanish_dubbed.z64> <out.z64>
  english.z64: the English translation (SHA-1 5945ff3935d9d9531bbb5cfcabed7c198e2b2f41)
  spanish_dubbed.z64: SAPESP.10.DOBLADO.xdelta applied to the Japanese ROM
    (SHA-1 99a6462e43f1685c49c60e0cc523ba92384edd43)
Load the result with Load ROM, as the translation.
"""
import hashlib
import struct
import sys

ENGLISH_SHA1 = "5945ff3935d9d9531bbb5cfcabed7c198e2b2f41"
DUBBED_SHA1 = "99a6462e43f1685c49c60e0cc523ba92384edd43"

SOUND_START = 0x0138D310   # the dub's sound data: its "N64 PTR by matsu" header
SOUND_END = 0x01FF2F80
FIRST_TABLE = 0x130        # the first wave table, from the start
NEW_START = 0x02000000     # past the 32 MB ROM

# The lui / addiu pairs that build the first wave table's offset (rom_version.cpp).
MAIN_VRAM = 0x80025C00
MAIN_ROM = 0x1000
PAIRS = [(0x8003AB28, 0x8003AB2C), (0x8003AB78, 0x8003AB7C), (0x8003ABA4, 0x8003ABA8), (0x8003ABF0, 0x8003ABF4)]

# The voice banks' ROM offsets (40 words in the main program's data), in both ROMs.
VOICE_TABLE = 0x399A8
VOICE_BANKS = 40


def sha1(data):
    return hashlib.sha1(data).hexdigest()


def main():
    if len(sys.argv) != 4:
        sys.exit(__doc__)
    english = bytearray(open(sys.argv[1], "rb").read())
    dubbed = open(sys.argv[2], "rb").read()
    if sha1(english) != ENGLISH_SHA1:
        sys.exit("The first ROM isn't the English translation (.z64, SHA-1 %s)." % ENGLISH_SHA1)
    if sha1(dubbed) != DUBBED_SHA1:
        sys.exit("The second ROM isn't the Spanish dub (.z64, SHA-1 %s)." % DUBBED_SHA1)
    sound = dubbed[SOUND_START:SOUND_END]
    if sound[0x10:0x20] != b"N64 PTR by matsu":
        sys.exit("The Spanish dub's sound data isn't where it should be.")

    out = english + bytes(NEW_START - len(english)) + sound
    out += bytes(-len(out) % 0x100000)
    table = NEW_START + FIRST_TABLE
    hi = (table + 0x8000) >> 16
    lo = table - (hi << 16)
    for lui_addr, addiu_addr in PAIRS:
        o = lui_addr - MAIN_VRAM + MAIN_ROM
        lui = struct.unpack(">I", out[o:o + 4])[0]
        out[o:o + 4] = struct.pack(">I", (lui & 0xFFFF0000) | hi)
        o = addiu_addr - MAIN_VRAM + MAIN_ROM
        addiu = struct.unpack(">I", out[o:o + 4])[0]
        out[o:o + 4] = struct.pack(">I", (addiu & 0xFFFF0000) | (lo & 0xFFFF))
    for i in range(VOICE_BANKS):
        o = VOICE_TABLE + i * 4
        bank = struct.unpack(">I", dubbed[o:o + 4])[0]
        if not SOUND_START <= bank < SOUND_END:
            sys.exit("The Spanish dub's voice table isn't where it should be.")
        out[o:o + 4] = struct.pack(">I", bank - SOUND_START + NEW_START)
    open(sys.argv[3], "wb").write(out)
    print("Wrote %s (%d MB): English text, Spanish voices." % (sys.argv[3], len(out) >> 20))


if __name__ == "__main__":
    main()
