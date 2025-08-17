#pragma once
#include <cstdint>
#include "types.hpp"

struct ZobristKeys {
    uint64_t piece[2][6][64]{};
    uint64_t castling[16]{};
    uint64_t epFile[8]{};
    uint64_t side{};
};

class SplitMix64 {
public:
    explicit SplitMix64(uint64_t seed) : x(seed) {}
    uint64_t next() {
        uint64_t z = (x += 0x9E3779B97F4A7C15ULL);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
        return z ^ (z >> 31);
    }
private:
    uint64_t x;
};

inline ZobristKeys init_zobrist() {
    ZobristKeys z{};
    SplitMix64 rng(0xC0FFEE123456789ULL);
    for (int c = 0; c < 2; ++c)
        for (int p = 0; p < 6; ++p)
            for (int s = 0; s < 64; ++s)
                z.piece[c][p][s] = rng.next();
    for (int i = 0; i < 16; ++i) z.castling[i] = rng.next();
    for (int f = 0; f < 8; ++f) z.epFile[f] = rng.next();
    z.side = rng.next();
    return z;
}