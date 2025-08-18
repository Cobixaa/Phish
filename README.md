## phish — UCI chess engine (C++)

phish is a fast, CPU-optimized UCI chess engine in modern C++20.

- Bitboard move generation with legal filtering (castling, en passant, promotions)
- Zobrist hashing and a transposition table
- Iterative deepening, alpha-beta with aspiration windows
- Null-move pruning, LMR, killers & history heuristics, quiescence search
- Tapered material + PST evaluation
- UCI compatible (works with GUIs including DroidFish)

### Quick start (Linux/macOS) — no CMake

Run this from the repository root to build a Release binary quickly:

```bash
clang++ -std=c++20 -O3 -DNDEBUG -march=native -mtune=native -flto -pthread \
  -I phish/include phish/src/*.cpp -o phish/phish
```

If you prefer GCC, replace `clang++` with `g++`.

### Build with CMake (Linux/macOS)

```bash
mkdir -p phish/build && cd phish/build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

The binary will be at `phish/build/phish`.

### Build (Windows)

From a Visual Studio Developer Prompt:

```bash
mkdir phish\build && cd phish\build
cmake -A x64 -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --config Release -j
```

Binary: `phish/build/Release/phish.exe`.

### Termux on Android (build on device)

1) Install toolchain and storage access:

```bash
pkg update && pkg upgrade -y
pkg install -y clang make cmake git
termux-setup-storage
```

2) Clone and build (fast, no CMake):

```bash
git clone https://github.com/your/repo.git && cd repo
clang++ -std=c++20 -O3 -DNDEBUG -march=native -flto -pthread \
  -I phish/include phish/src/*.cpp -o phish/phish
```

3) Import into DroidFish:

- Copy the binary to shared storage:

```bash
cp -f phish/phish ~/storage/shared/Download/phish
```

- In DroidFish: Options → Manage Chess Engines → Import Engine → pick the copied `phish` binary.

Notes:
- On newer Android versions, use the Storage Access Framework (SAF) in DroidFish to browse to `Download/`.
- `-march=native` uses your device CPU features automatically. If you see issues, remove it.

### Run (UCI)

```bash
./phish/phish
```

Send UCI commands, for example:

- `uci`
- `isready`
- `position startpos`
- `go depth 10`

### Options

- **Hash**: size of the transposition table in MB (default 64). Increase for strength if you have RAM.

### Install in GUIs

- **Cute Chess (desktop)**: Add engine → point to the `phish` binary. Set options (Hash).
- **Arena (Windows)**: Engines → Install New Engine → select `phish.exe` (UCI).
- **DroidFish (Android)**: Import the `phish` binary as described in the Termux section above.

### Performance

Already enabled in Release builds:

- `-O3 -DNDEBUG`, `-march=native -mtune=native`, `-flto`

Tips:

- Increase `Hash` to 256–1024 MB for long analysis sessions.
- Build with the latest Clang or GCC for better codegen/LTO.
- Optional: try `-Ofast` or disabling RTTI/exceptions (`-fno-rtti -fno-exceptions`) if your toolchain supports it. These can reduce binary size and slightly improve speed; use only if you are comfortable with the trade‑offs.

### Example UCI session

```bash
uci
isready
position startpos moves e2e4 e7e5 g1f3 b8c6
go wtime 300000 btime 300000 winc 2000 binc 2000
```

### License

Apache License 2.0