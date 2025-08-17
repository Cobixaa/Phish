# phish - UCI chess engine (C++)

phish is a fast, CPU-optimized UCI chess engine written in modern C++20. It features:
- Bitboard move generation with legal filtering (castling, en passant, promotions)
- Zobrist hashing, transposition table
- Iterative deepening, alpha-beta with aspiration windows
- Null move pruning, late move reductions, killers & history heuristics, quiescence search
- Tapered material+PST evaluation
- UCI protocol compatible (works in GUIs including DroidFish)

## Build (Linux/macOS)

Prereqs:
- CMake >= 3.16
- Clang or GCC with C++20 support

Steps:
- mkdir -p build && cd build
- cmake -DCMAKE_BUILD_TYPE=Release ..
- cmake --build . --config Release -j

Binary is at `build/phish`.

## Build (Windows)

- Install CMake and Visual Studio (with C++ toolchain)
- From a Developer Prompt:
  - mkdir build && cd build
  - cmake -A x64 -DCMAKE_BUILD_TYPE=Release ..
  - cmake --build . --config Release -j

Binary is at `build/Release/phish.exe`.

## Run (UCI)

- ./phish
- Send UCI commands (example):
  - uci
  - isready
  - position startpos
  - go depth 10

## Options

- Hash: size of transposition table in MB (default 64). Increase for strength if you have RAM.

## Install in GUIs

### Cute Chess (desktop)
- Add engine: point to `phish` binary
- Set options (Hash)

### Arena (Windows)
- Engines -> Install New Engine -> select `phish.exe`
- Protocol: UCI

### DroidFish (Android)
Two options:
1) Place binary in DroidFish engines folder (rooted or via SAF):
   - Build for Android (ARM64) via NDK or termux (clang)
   - Copy the binary (e.g., `phish`) to `DroidFish/uci` (varies by device)
   - In DroidFish: Options -> Manage Chess Engines -> Import Engine -> select `phish`
2) Use an `OBK` engine package:
   - Package `phish` binary and a minimal manifest; import in DroidFish

Quick test in DroidFish:
- Start analysis; engine should display best move and PV lines

Note: This repo builds a native Linux binary. For Android, compile with clang targeting `aarch64-linux-android` using the Android NDK or termux:
- In termux on device: `pkg install clang cmake make`
- Build the same steps; copy `phish` into DroidFish engine directory via Android file manager (Storage Access Framework)

## Performance and optimization

CMake enables:
- `-O3 -DNDEBUG`, `-march=native -mtune=native`, `-flto` in Release builds (Clang/GCC)

Engine-side optimizations:
- Null-move pruning (depth-aware)
- Aspiration windows to reduce re-search
- Late-move reductions and pruning for quiet moves
- Transposition table with replacement-on-depth
- Killer and history heuristics for move ordering
- Minimal allocations; tight data structures

Tuning tips:
- Increase `Hash` to 256-1024 MB for long analysis
- Pin CPU governor to performance where applicable
- Build with the latest Clang for better LTO and codegen

## Example UCI session

```
uci
isready
position startpos moves e2e4 e7e5 g1f3 b8c6
go wtime 300000 btime 300000 winc 2000 binc 2000
```

## License

Apache License 2.0