#include "tt.hpp"
#include <cstring>

static inline uint16_t pack_move16(Move m) {
    // 6 bits from, 6 bits to, 4 bits promo
    return static_cast<uint16_t>((m.from() & 63) | ((m.to() & 63) << 6) | ((m.promo() & 15) << 12));
}

static inline Move unpack_move16(uint16_t v) {
    int from = v & 63; int to = (v >> 6) & 63; int promo = (v >> 12) & 15;
    if (promo == 0) return Move::make(from, to, NO_PIECE_TYPE);
    return Move::make(from, to, NO_PIECE_TYPE, NO_PIECE_TYPE, static_cast<PieceType>(promo));
}

void TranspositionTable::resize_mb(size_t mb) {
    size_t bytes = mb * 1024ULL * 1024ULL;
    size_t n = bytes / sizeof(TTEntry);
    size_t pow2 = 1; while (pow2 < n) pow2 <<= 1; n = pow2 ? pow2 : 1;
    table.clear(); table.resize(n);
    mask = n - 1;
    clear();
}

void TranspositionTable::clear() {
    std::memset(table.data(), 0, table.size() * sizeof(TTEntry));
}

void TranspositionTable::store(uint64_t key, int depth, int score, int staticEval, Bound bound, Move best) {
    TTEntry &e = table[key & mask];
    if (e.key != key || depth >= e.depth) {
        e.key = key;
        e.depth = static_cast<uint8_t>(depth);
        e.score = static_cast<int16_t>(score);
        e.staticEval = static_cast<int16_t>(staticEval);
        e.bound = bound;
        e.bestMove = pack_move16(best);
    }
}

bool TranspositionTable::probe(uint64_t key, TTEntry &out) const {
    const TTEntry &e = table[key & mask];
    if (e.key == key) { out = e; return true; }
    return false;
}