#pragma once
#include <cstdint>
#include <vector>
#include <atomic>
#include "types.hpp"

enum class Bound : uint8_t { EXACT = 0, LOWER = 1, UPPER = 2 };

struct TTEntry {
    uint64_t key;
    int16_t score;
    int16_t staticEval;
    uint16_t bestMove; // pack Move lower 16 bits (from 6, to 6, promo 4)
    uint8_t depth;
    Bound bound;
};

class TranspositionTable {
public:
    TranspositionTable() { resize_mb(64); }
    void resize_mb(size_t mb);
    void clear();

    void store(uint64_t key, int depth, int score, int staticEval, Bound bound, Move best);
    bool probe(uint64_t key, TTEntry &out) const;

private:
    std::vector<TTEntry> table;
    size_t mask = 0;
};