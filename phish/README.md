# phish - UCI chess engine (C++)

Build:

- mkdir -p build && cd build
- cmake -DCMAKE_BUILD_TYPE=Release ..
- cmake --build . --config Release -j

Run (UCI):

- ./phish

The engine speaks UCI and can be loaded in GUIs like DroidFish or CuteChess. Configure the Hash option to allocate transposition table memory.