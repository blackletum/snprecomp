"""Code segments of Tsumi to Batsu (Japan) and the fan-translated English ROM.

The two ROMs share every code segment byte for byte (STATUS.md round 1).

- main: loaded by IPL3 to the entry point. Text ends at 0x80056E80, where the
  RSP microcode (data) starts; the Yay0 decoder at 0x80056DD0 is the last
  function.
- the game's own load table at ROM 0x34BAC (VRAM 0x800597AC), 20 bytes per
  entry: rom start, vram start, text end, data end, bss end. Entry 0 ("seg0")
  is a second static segment right after main's BSS; entries 1-23 are
  overlays sharing VRAM 0x800E4780; 24-27 share 0x802B4000 / 0x802C3B30
  (24 has no code). The table ends at the first entry with vram 0.
"""
import struct

MAIN = dict(name="main", rom=0x1000, vram=0x80025C00, text=0x31280)
LOAD_TABLE_ROM = 0x34BAC


def load_table(rom):
    entries = []
    p = LOAD_TABLE_ROM
    while True:
        r, v, text_end, data_end, bss_end = struct.unpack(">5I", rom[p:p + 20])
        if r == 0 or v == 0:
            return entries
        entries.append(dict(rom=r, vram=v, text_end=text_end, data_end=data_end, bss_end=bss_end))
        p += 20


# Main's data: from the end of its text to the start of BSS.
MAIN_DATA_END = 0x80063450


def segments(rom):
    """Code segments: name, rom, vram, text size, and data (size of the data after the text)."""
    segs = [dict(MAIN, data=MAIN_DATA_END - MAIN["vram"] - MAIN["text"])]
    for i, e in enumerate(load_table(rom)):
        if e["text_end"] > e["vram"]:
            segs.append(dict(name="seg0" if i == 0 else "ovl%02d" % i,
                             rom=e["rom"], vram=e["vram"], text=e["text_end"] - e["vram"],
                             data=e["data_end"] - e["text_end"]))
    return segs
