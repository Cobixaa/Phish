## phish — UCI chess engine (C++)

phish is a fast, CPU-optimized UCI chess engine in modern C++20.

- Bitboard move generation with legal filtering (castling, en passant, promotions)
- Zobrist hashing and a transposition table
- Iterative deepening, alpha-beta with aspiration windows
- Null-move pruning, LMR, killers & history heuristics, quiescence search
- Tapered material + PST evaluation
- UCI compatible (works with GUIs including DroidFish)

### Quick build (inside this `phish/` directory, no CMake)

```bash
clang++ -std=c++20 -O3 -DNDEBUG -march=native -mtune=native -flto -pthread \
  -I include src/*.cpp -o phish
```

Or with GCC: replace `clang++` with `g++`.

### Build with CMake

```bash
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . -j
```

Binary: `build/phish` (or `build/Release/phish.exe` on Windows).

### Termux on Android (build on device)

```bash
pkg update && pkg upgrade -y
pkg install -y clang make cmake git
termux-setup-storage
clang++ -std=c++20 -O3 -DNDEBUG -march=native -flto -pthread \
  -I include src/*.cpp -o phish
cp -f phish ~/storage/shared/Download/phish
```

In DroidFish: Options → Manage Chess Engines → Import Engine → select the copied `phish`.

### Run (UCI)

```bash
./phish
```

Then send UCI commands, e.g. `uci`, `isready`, `position startpos`, `go depth 10`.

### Options

- **Hash**: size of the transposition table in MB (default 64).

### Performance

Release builds enable `-O3 -DNDEBUG -march=native -mtune=native -flto`.

Tips:

- Increase `Hash` to 256–1024 MB for long analysis.
- Consider `-Ofast` and/or `-fno-rtti -fno-exceptions` where appropriate.

### Example UCI session

```bash
uci
isready
position startpos moves e2e4 e7e5 g1f3 b8c6
go wtime 300000 btime 300000 winc 2000 binc 2000
```

### License

Apache License 2.0