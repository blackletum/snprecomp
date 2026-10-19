"""List busy-wait loops: short backward loops that only load memory and branch.

    python3 tools/find_spin_loops.py <rom.z64>

On the N64 an interrupt preempts such a loop when the thread it waits on
should run. Loops in SDK functions the runtime replaces (hardware register
polls in osPiRawStartDma and friends) are listed too and need nothing. The runtime only switches threads inside OS calls, so each of
these needs a yield hook in snp.jp.toml (STATUS.md round 2). Prints
segment, function-relative loop head and the loop's instructions.
"""
import os
import struct
import sys

import rabbitizer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from segments import segments

MAX_LOOP = 8  # instructions, including the branch and its delay slot

LOADS = {rabbitizer.InstrId.cpu_lw, rabbitizer.InstrId.cpu_lh, rabbitizer.InstrId.cpu_lhu,
         rabbitizer.InstrId.cpu_lb, rabbitizer.InstrId.cpu_lbu}
# No address arithmetic: a loop that advances a pointer or a counter is a
# string or checksum loop, not a wait.
HARMLESS = {rabbitizer.InstrId.cpu_lui, rabbitizer.InstrId.cpu_nop, rabbitizer.InstrId.cpu_andi,
            rabbitizer.InstrId.cpu_sltiu, rabbitizer.InstrId.cpu_slti}


def main():
    rom = open(sys.argv[1], "rb").read()
    for seg in segments(rom):
        words = struct.unpack(">%dI" % (seg["text"] // 4), rom[seg["rom"]:seg["rom"] + seg["text"]])
        for i, word in enumerate(words):
            vram = seg["vram"] + i * 4
            instr = rabbitizer.Instruction(word, vram)
            if not instr.isBranch():
                continue
            target = instr.getBranchVramGeneric()
            if not (vram - (MAX_LOOP - 2) * 4 <= target <= vram):
                continue
            body = [rabbitizer.Instruction(words[(target - seg["vram"]) // 4 + k], target + k * 4)
                    for k in range((vram - target) // 4 + 2)]
            ids = [b.uniqueId for b in body[:-2]] + [body[-1].uniqueId]
            if not any(x in LOADS for x in ids):
                continue
            if all(x in LOADS or x in HARMLESS for x in ids):
                print("%s 0x%08X  %s" % (seg["name"], target, " ; ".join(b.disassemble() for b in body)))


if __name__ == "__main__":
    main()
