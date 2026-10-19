"""Build the N64Recomp symbol file from spimdisasm's function scan.

    python3 tools/gen_syms_toml.py <rom.z64> <spim_dir> syms/n64sym_jp.txt SnPSyms/snp.jp.syms.toml

- One [[section]] per code segment (tools/segments.py), covering its text.
- Function starts and sizes come from spimdisasm (tools/run_spimdisasm.py).
- libultra / audio library names come from n64sym's signature scan
  (syms/n64sym_jp.txt). Names with _text_ are offsets inside a function and
  are skipped; a named start spimdisasm missed becomes a split point.
- Functions only reached through a pointer (built with lui/addiu, or a word
  in a segment's data) are often missed by spimdisasm; a function starts at
  such an address when it begins a stack frame and the code before it never
  falls through. (The stack frame rule keeps out jump table case labels and
  addresses made up by a lui and an unrelated addiu.)
- N64Recomp can't branch into another function's interior, which
  hand-written code sharing tails does (libgcc, __osException). The target
  function is split there when possible, else the two are merged
  (fix_interior_branches).
- Main and seg0 have unique addresses, so their functions are func_XXXXXXXX.
  Overlays share VRAM, so theirs are func_ovlNN_XXXXXXXX.
- Hand corrections go in the OVERRIDES table below, so regenerating never
  loses them.
"""
import csv
import os
import struct
import sys

import rabbitizer

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from segments import segments

# (section name, vram) -> size, or None to drop the entry. Filled in as
# functions are found mis-sized; see STATUS.md.
OVERRIDES = {
    # The entry point stub (BSS clear, stack) ends in `jr $t2` to
    # 0x80025C40, which spimdisasm took as part of the same function.
    ("main", 0x80025C00): 0x40,
    ("main", 0x80025C40): 0x64,
    # __osException's tail (the interrupt dispatch) was split off as its own
    # function; the runtime replaces the whole handler, so keep it one piece.
    ("main", 0x8003D940): 0x66C,
    ("main", 0x8003DEBC): None,
}


# SDK functions n64sym's signatures missed, named from what they call
# (STATUS.md round 1). N64Recomp's built-in lists replace or skip them by name.
EXTRA_NAMES = {
    0x8003EBB0: "osEPiStartDma",      # devmgr active check, then jam/send of the request
    0x8003F350: "osAiGetLength",      # reads AI_LEN (0xA4500004), between __osAiDeviceBusy and osAiGetStatus
    0x800433B0: "osContInit",         # __osPackRequestData, __osSiRawStartDma, __osContGetInitData
    0x80048110: "viMgrMain",          # __osViGetCurrentContext, __osViSwapContext, __osTimerInterrupt
    0x8004ABF8: "__osRepairPackId",   # between __osIdCheckSum and __osCheckPackId; __osContRam*
}


def read_spim(path):
    funcs = {}
    with open(path) as f:
        for row in csv.DictReader(f):
            funcs[int(row["address"], 16)] = int(row["length"], 16)
    return funcs


def read_n64sym(path):
    names = {}
    for line in open(path):
        parts = line.split()
        if len(parts) != 2 or "_text_" in parts[1]:
            continue
        names.setdefault(int(parts[0], 16), parts[1])
    return names


def outside_branch_targets(rom, rom_start, vram, size):
    """Targets of branches/jumps in [vram, vram+size) that land outside it."""
    targets = []
    for off in range(0, size, 4):
        word = struct.unpack(">I", rom[rom_start + off:rom_start + off + 4])[0]
        instr = rabbitizer.Instruction(word, vram + off)
        if instr.isBranch() or instr.uniqueId == rabbitizer.InstrId.cpu_j:
            target = instr.getBranchVramGeneric() if instr.isBranch() else instr.getInstrIndexAsVram()
            if not (vram <= target < vram + size):
                targets.append(target)
    return targets


def jal_targets(rom, segs):
    targets = set()
    for seg in segs:
        for off in range(0, seg["text"], 4):
            word = struct.unpack(">I", rom[seg["rom"] + off:seg["rom"] + off + 4])[0]
            if word >> 26 == 3:  # jal
                targets.add(((seg["vram"] + off) & 0xF0000000) | ((word & 0x3FFFFFF) << 2))
    return targets


def word_at(rom, seg, vram):
    off = seg["rom"] + vram - seg["vram"]
    return struct.unpack(">I", rom[off:off + 4])[0]


def is_terminator(word):
    """Unconditional b / j / jr: execution never falls through past its delay slot."""
    op = word >> 26
    if op == 2:  # j
        return True
    if op == 4 and (word >> 16) & 0x3FF == 0:  # beq $zero, $zero (b)
        return True
    return op == 0 and word & 0x3F == 8  # jr


def branch_target(word, vram):
    instr = rabbitizer.Instruction(word, vram)
    if instr.isBranch():
        return instr.getBranchVramGeneric()
    if instr.uniqueId == rabbitizer.InstrId.cpu_j:
        return instr.getInstrIndexAsVram()
    return None


def self_contained_tail(rom, seg, target, limit):
    """Size of the code from target up to its first unconditional exit, if every
    branch in it stays inside it; else None. Such a tail can be compiled as its
    own (overlapping) function, and branches into it become tail calls."""
    vram = target
    while vram + 4 < limit:
        word = word_at(rom, seg, vram)
        t = branch_target(word, vram)
        if t is not None and not (target <= t < limit):
            return None
        if is_terminator(word):
            end = vram + 8
            for v in range(target, end, 4):
                t = branch_target(word_at(rom, seg, v), v)
                if t is not None and not (target <= t < end):
                    return None
            return end - target
        vram += 4
    return None


def pointer_targets(rom, seg, all_segs):
    """Code addresses this segment's code builds with lui + addiu/ori, or stores
    as words in its data. Returns (code_refs, data_runs): code_refs maps
    target -> segment name; data_runs is a list of runs of consecutive data
    words, each a list of (target, segment name). A target in main or seg0
    (unique addresses) belongs to that segment, one in the overlay range to
    this segment itself."""
    static = [s for s in all_segs if s["name"] in ("main", "seg0")]

    def owner(addr):
        for s in static:
            if s["vram"] <= addr < s["vram"] + s["text"]:
                return s["name"]
        if seg["name"] not in ("main", "seg0") and seg["vram"] <= addr < seg["vram"] + seg["text"]:
            return seg["name"]
        return None

    code_refs = {}
    hi = {}
    for off in range(0, seg["text"], 4):
        word = struct.unpack(">I", rom[seg["rom"] + off:seg["rom"] + off + 4])[0]
        op, rs, rt, imm = word >> 26, (word >> 21) & 31, (word >> 16) & 31, word & 0xFFFF
        if op == 0x0F:  # lui
            hi[rt] = imm << 16
        elif op in (0x09, 0x0D) and rs in hi:  # addiu / ori
            addr = (hi[rs] + (imm - 0x10000 if op == 0x09 and imm & 0x8000 else imm)) & 0xFFFFFFFF
            if owner(addr):
                code_refs[addr] = owner(addr)
    data_runs = []
    run = []
    data_start = seg["rom"] + seg["text"]
    for off in range(0, seg.get("data", 0) & ~3, 4):
        addr = struct.unpack(">I", rom[data_start + off:data_start + off + 4])[0]
        if addr & 3 == 0 and owner(addr):
            run.append((addr, owner(addr)))
        elif run:
            data_runs.append(run)
            run = []
    if run:
        data_runs.append(run)
    return code_refs, data_runs


def split_at_pointer_targets(rom, seg, funcs, targets):
    """Start a function at each pointer target inside another function where the
    code before it never falls through (an unconditional exit and its delay
    slot): functions only reached through a pointer, which spimdisasm missed."""
    for target in sorted(targets):
        if target in funcs:
            continue
        owners = [a for a in funcs if a < target < a + funcs[a]]
        if not owners:
            continue
        owner = owners[0]
        if target - 8 >= owner and is_terminator(word_at(rom, seg, target - 8)):
            funcs[target] = owner + funcs[owner] - target
            funcs[owner] = target - owner


def starts_stack_frame(rom, seg, vram):
    word = word_at(rom, seg, vram)
    return word >> 16 == 0x27BD and word & 0x8000  # addiu $sp, $sp, -N


def data_pointer_targets(rom, seg, runs):
    """The targets in this segment from data words that begin with a stack
    frame (addiu $sp, $sp, -N). Data also holds jump tables, whose case labels
    look like any other code address but never start a stack frame. (A leaf
    function reached only from data is missed this way: add it to OVERRIDES.)"""
    targets = set()
    for run in runs:
        for t, name in run:
            if name == seg["name"] and starts_stack_frame(rom, seg, t):
                targets.add(t)
    return targets


def fix_interior_branches(rom, seg, funcs, dropped, tails):
    """Make every branch land on a function start or inside its own function.

    A branch from one function into the interior of another (hand-written
    code sharing tails) is fixed by splitting the target function there when
    the code before it never falls through (N64Recomp then emits a tail
    call), by compiling a self-contained tail starting there as its own
    overlapping function (tails: addr -> size), and otherwise by merging the
    two. funcs: addr -> size. Starts that
    disappear into a merged function go to dropped (addr -> size)."""
    for allow_split in (True, False):
        changed = True
        while changed:
            changed = False
            addrs = sorted(funcs)
            for addr in addrs:
                size = funcs[addr]
                rom_start = seg["rom"] + addr - seg["vram"]
                for target in outside_branch_targets(rom, rom_start, addr, size):
                    if target in funcs or target in tails:
                        continue  # tail call to a function start: fine
                    owners = [a for a in addrs if a <= target < a + funcs[a]]
                    if not owners:
                        continue
                    owner = owners[0]
                    tail = self_contained_tail(rom, seg, target, owner + funcs[owner]) if allow_split else None
                    if tail:
                        tails[target] = tail
                    elif allow_split and target - 8 >= owner and is_terminator(word_at(rom, seg, target - 8)):
                        funcs[target] = owner + funcs[owner] - target
                        funcs[owner] = target - owner
                    elif not allow_split:
                        lo = min(addr, owner)
                        hi = max(addr + size, owner + funcs[owner])
                        for a in [a for a in addrs if lo <= a < hi and a != lo]:
                            dropped.setdefault(a, funcs[a])
                            del funcs[a]
                        funcs[lo] = hi - lo
                    else:
                        continue
                    changed = True
                    break
                if changed:
                    break
    return funcs


def main():
    rom_path, spim_dir, n64sym_path, out_path = sys.argv[1:5]
    rom = open(rom_path, "rb").read()
    all_segs = segments(rom)
    all_jal_targets = jal_targets(rom, all_segs)
    code_ptr_targets = {}
    data_runs = []
    for seg in all_segs:
        code_refs, runs = pointer_targets(rom, seg, all_segs)
        for addr, name in code_refs.items():
            code_ptr_targets.setdefault(name, set()).add(addr)
        data_runs += runs
    lib_names = read_n64sym(n64sym_path)
    lib_names.update(EXTRA_NAMES)
    used_names = set()
    out = []
    out.append("# Tsumi to Batsu: Hoshi no Keishousha (Japan) symbol table for N64Recomp.\n")
    out.append("# Generated by tools/gen_syms_toml.py -- edit that script (OVERRIDES), not this file.\n")
    total = 0
    for seg in segments(rom):
        start, end = seg["vram"], seg["vram"] + seg["text"]
        funcs = read_spim(os.path.join(spim_dir, seg["name"] + ".csv"))
        named = seg["name"] in ("main", "seg0")
        if named:
            for addr in lib_names:
                if start <= addr < end and addr not in funcs:
                    funcs[addr] = 0
        addrs = sorted(funcs)
        for i, addr in enumerate(addrs):
            nxt = addrs[i + 1] if i + 1 < len(addrs) else end
            funcs[addr] = min(funcs[addr] or nxt - addr, nxt - addr)
        for key, size in OVERRIDES.items():
            if key[0] == seg["name"]:
                if size is None:
                    funcs.pop(key[1], None)
                else:
                    funcs[key[1]] = size
        targets = {t for t in code_ptr_targets.get(seg["name"], set()) if starts_stack_frame(rom, seg, t)}
        targets |= data_pointer_targets(rom, seg, data_runs)
        split_at_pointer_targets(rom, seg, funcs, targets)
        dropped = {}
        tails = {}
        funcs = fix_interior_branches(rom, seg, funcs, dropped, tails)
        addrs = sorted(funcs)
        out.append("\n[[section]]\nname = \"%s\"\nrom = 0x%06X\nvram = 0x%08X\nsize = 0x%X\n\nfunctions = [\n"
                   % (seg["name"], seg["rom"], seg["vram"], seg["text"]))
        for addr in addrs:
            size = funcs[addr]
            if size <= 0:
                continue
            if named:
                name = lib_names.get(addr, "func_%08X" % addr)
            else:
                name = "func_%s_%08X" % (seg["name"], addr)
            if name in used_names:
                name = "%s_%08X" % (name, addr)
            used_names.add(name)
            out.append("    { name = \"%s\", vram = 0x%08X, size = 0x%X },\n" % (name, addr, size))
            total += 1
        for addr, size in sorted(tails.items()):
            name = "func_%08X" % addr if named else "func_%s_%08X" % (seg["name"], addr)
            out.append("    { name = \"%s\", vram = 0x%08X, size = 0x%X }, # shared tail, also inside the function before it\n" % (name, addr, size))
            total += 1
        # Entry points inside a merged function that something calls directly
        # stay callable as their own (overlapping) function.
        for addr in sorted(dropped):
            if addr in all_jal_targets:
                # Runs to the end of the merged function containing it.
                owner = max(a for a in funcs if a < addr)
                size = owner + funcs[owner] - addr
                name = lib_names.get(addr, "func_%08X" % addr) if named else "func_%s_%08X" % (seg["name"], addr)
                out.append("    { name = \"%s\", vram = 0x%08X, size = 0x%X }, # inside a merged function\n" % (name, addr, size))
                total += 1
        out.append("]\n")
    open(out_path, "w").write("".join(out))
    print("wrote %d functions to %s" % (total, out_path))


if __name__ == "__main__":
    main()
