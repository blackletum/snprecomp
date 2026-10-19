"""Find function boundaries in every code segment with spimdisasm.

    python3 tools/run_spimdisasm.py <rom.z64> <out_dir>

Writes <out_dir>/<segment>.csv (spimdisasm --function-info) per segment;
tools/gen_syms_toml.py turns them into the N64Recomp symbol file. Segments
whose .csv already exists are skipped.
"""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from segments import segments


def main():
    rom_path, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    rom = open(rom_path, "rb").read()
    for s in segments(rom):
        csv = os.path.join(out, s["name"] + ".csv")
        if os.path.exists(csv):
            continue
        print(s["name"], hex(s["rom"]), hex(s["vram"]), hex(s["text"]), flush=True)
        subprocess.run([sys.executable, "-m", "spimdisasm.singleFileDisasm", rom_path, os.path.join(out, s["name"]),
                        "--start", hex(s["rom"]), "--end", hex(s["rom"] + s["text"]), "--vram", hex(s["vram"]),
                        "--function-info", csv, "--compiler", "GCC", "--no-write-binary"],
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=True)


if __name__ == "__main__":
    main()
