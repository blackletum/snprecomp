# Building Guide

You need your own copy of the game: the Japanese ROM, Tsumi to Batsu:
Hoshi no Keishousha (`syms/rom_info.md`). The game code is recompiled from it
locally and is never committed to this repository. (The fan-translated
English ROM runs the same recompiled code; you only need it to play in
English, not to build.)

## 1. Clone with submodules

```bash
git clone --recurse-submodules https://github.com/wootbeer/snprecomp.git
# if you forgot --recurse-submodules:
git submodule update --init --recursive
```

This pulls in `lib/N64ModernRuntime`, `lib/RecompFrontend` and `lib/rt64`
(plus their nested submodules), several hundred MB in all. They're pinned to
the same commits as btgarecomp, and `lib-patches/` carries its RT64 fixes.

## 2. Install dependencies

### Windows

- Visual Studio 2022 with the "Desktop development with C++" workload,
  including the "C++ Clang Compiler for Windows" and "C++ CMake tools for
  Windows" components.
- WSL with `clang`, `lld` and `make` installed in it (for Ubuntu:
  `sudo apt install clang lld make`). The small MIPS patch library in
  `patches/` has to be cross-compiled for the N64, and the Windows builds of
  clang don't include the MIPS backend, so CMake runs that one step through
  `wsl.exe`.

### Linux (Ubuntu/Debian)

```bash
sudo apt-get install cmake ninja-build libsdl2-dev libgtk-3-dev libvulkan-dev lld llvm clang
```

## 3. Put the ROM in place

Place the Japanese ROM at the repo root, named exactly
`Tsumi to Batsu - Hoshi no Keishousha (Japan).z64`. The recompilers read it
from there (`snp.jp.toml`, `n_aspMain.jp.toml`).

It must be big-endian `.z64` (header bytes `80 37 12 40`). To convert a
`.n64` or `.v64` dump and check it:

```bash
python3 tools/normalize_rom.py "your dump.n64" "Tsumi to Batsu - Hoshi no Keishousha (Japan).z64"
```

The game itself asks for a ROM on first launch (`.z64`, `.n64` or `.v64`)
and accepts either the Japanese ROM or the fan translation.

## 4. Configure, generate the recompiled code, build

Configuring applies this project's fixes to the submodules (`lib-patches/`).
One of them changes the code N64Recomp generates (division by zero), so the
game code must be generated with the N64Recomp this build makes, not one
built from the submodule on its own. Generating writes `RecompiledFuncs/`
(gitignored); after this first time, the build reruns N64Recomp by itself
whenever the `.toml`, the symbol file or an N64Recomp fix changes. The build
also generates the RSP audio microcode (`rsp/`) and the patch library
(`RecompiledPatches/`).

**Linux:**

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
cmake --build build --target N64RecompCLI -j$(nproc)
./build/N64Recomp snp.jp.toml
cmake -S . -B build    # again, now that RecompiledFuncs/ exists
cmake --build build --target SnPRecompiled -j$(nproc)
./build/SnPRecompiled
```

**Windows** (in a Developer PowerShell for VS 2022, x64):

```powershell
$env:SNP_CLANGCL = "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\x64\bin\clang-cl.exe"
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_COMPILER="$env:SNP_CLANGCL" -DCMAKE_CXX_COMPILER="$env:SNP_CLANGCL"
cmake --build build --target N64RecompCLI
.\build\N64Recomp.exe snp.jp.toml
cmake -S . -B build
cmake --build build --target SnPRecompiled
.\build\SnPRecompiled.exe
```

Run it from the repo root: the game loads `assets/` relative to the working
directory. `--game snp` skips the launcher (`--game snp.n64.jp.1.0` or
`--game snp.n64.en.fan` to pick the ROM when both are set up).

## Regenerating the symbol file

`SnPSyms/snp.jp.syms.toml` is generated; don't edit it by hand. To rebuild it
(needs `pip install rabbitizer spimdisasm`):

```bash
python3 tools/run_spimdisasm.py "Tsumi to Batsu - Hoshi no Keishousha (Japan).z64" syms/spimdisasm
python3 tools/gen_syms_toml.py "Tsumi to Batsu - Hoshi no Keishousha (Japan).z64" syms/spimdisasm syms/n64sym_jp.txt SnPSyms/snp.jp.syms.toml
```

`run_spimdisasm.py` skips segments whose `.csv` is already in
`syms/spimdisasm/`; delete one to rescan it. Hand corrections go in the
`OVERRIDES` table in `tools/gen_syms_toml.py`.
