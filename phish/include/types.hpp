#pragma once
#include <cstdint>
#include <array>
#include <vector>
#include <limits>
#include <string>

using Bitboard = unsigned long long; // 64-bit

constexpr int BOARD_SQUARES = 64;
constexpr int NUM_COLORS = 2;
constexpr int NUM_PIECE_TYPES = 6;

enum Color : int { WHITE = 0, BLACK = 1, NO_COLOR = 2 };

enum PieceType : int { PAWN = 0, KNIGHT = 1, BISHOP = 2, ROOK = 3, QUEEN = 4, KING = 5, NO_PIECE_TYPE = 6 };

// Piece encoding: 0..11 valid, 12 = empty
// piece = color * 8 + pieceType (lower bits for type for quick masking). We leave room for flags if needed
constexpr int PIECE_EMPTY = 12;

inline constexpr int make_piece(Color c, PieceType pt) { return static_cast<int>(c) * 8 + static_cast<int>(pt); }
inline constexpr Color piece_color(int piece) { return piece == PIECE_EMPTY ? NO_COLOR : (piece < 8 ? WHITE : BLACK); }
inline constexpr PieceType piece_type(int piece) { return piece == PIECE_EMPTY ? NO_PIECE_TYPE : static_cast<PieceType>(piece & 7); }

enum Castling : int { WK = 1, WQ = 2, BK = 4, BQ = 8 };

inline constexpr int file_of(int sq) { return sq & 7; }
inline constexpr int rank_of(int sq) { return sq >> 3; }
inline constexpr int make_square(int file, int rank) { return (rank << 3) | file; }

inline constexpr Bitboard bit(int sq) { return 1ULL << sq; }

inline constexpr Bitboard north(Bitboard b) { return b << 8; }
inline constexpr Bitboard south(Bitboard b) { return b >> 8; }

inline constexpr Bitboard east(Bitboard b) { return (b & 0xFEFEFEFEFEFEFEULL) << 1; }
inline constexpr Bitboard west(Bitboard b) { return (b & 0x7F7F7F7F7F7F7F7FULL) >> 1; }

inline constexpr Bitboard northeast(Bitboard b) { return (b & 0xFEFEFEFEFEFEFEULL) << 9; }
inline constexpr Bitboard northwest(Bitboard b) { return (b & 0x7F7F7F7F7F7F7F7FULL) << 7; }
inline constexpr Bitboard southeast(Bitboard b) { return (b & 0xFEFEFEFEFEFEFEULL) >> 7; }
inline constexpr Bitboard southwest(Bitboard b) { return (b & 0x7F7F7F7F7F7F7F7FULL) >> 9; }

inline constexpr Bitboard file_mask(int file) {
    return 0x0101010101010101ULL << file;
}

inline constexpr Bitboard rank_mask(int rank) {
    return 0xFFULL << (rank * 8);
}

inline int popcount(Bitboard b) { return __builtin_popcountll(b); }
inline int lsb(Bitboard b) { return b ? __builtin_ctzll(b) : -1; }
inline int msb(Bitboard b) { return b ? 63 - __builtin_clzll(b) : -1; }

inline int pop_lsb(Bitboard &b) { int s = lsb(b); b &= b - 1ULL; return s; }

inline Color opposite(Color c) { return c == WHITE ? BLACK : WHITE; }

constexpr int INF = 30000;
constexpr int CHECKMATE = 29000;
constexpr int DRAW_SCORE = 0;

struct Move {
    // Encoded move representation
    // bits: 0-5 from, 6-11 to, 12-14 promType, 15 capture, 16 ep, 17 castle, 18 doublePush, 19 promo, 20-22 movedType, 23-26 capturedType, 27 reserved
    uint32_t v {0};

    Move() = default;
    explicit Move(uint32_t vv) : v(vv) {}

    static Move make(int from, int to, PieceType moved, PieceType captured = NO_PIECE_TYPE,
                     PieceType promo = NO_PIECE_TYPE, bool isCapture = false, bool isEP = false,
                     bool isCastle = false, bool isDoublePush = false) {
        uint32_t mv = 0;
        mv |= (from & 63);
        mv |= (to & 63) << 6;
        mv |= (static_cast<uint32_t>(promo) & 7) << 12;
        mv |= (isCapture ? 1U : 0U) << 15;
        mv |= (isEP ? 1U : 0U) << 16;
        mv |= (isCastle ? 1U : 0U) << 17;
        mv |= (isDoublePush ? 1U : 0U) << 18;
        mv |= ((promo != NO_PIECE_TYPE) ? 1U : 0U) << 19;
        mv |= (static_cast<uint32_t>(moved) & 7) << 20;
        mv |= (static_cast<uint32_t>(captured) & 7) << 23;
        return Move(mv);
    }

    int from() const { return v & 63; }
    int to() const { return (v >> 6) & 63; }
    PieceType promo() const { return static_cast<PieceType>((v >> 12) & 7); }
    bool is_capture() const { return (v >> 15) & 1U; }
    bool is_ep() const { return (v >> 16) & 1U; }
    bool is_castle() const { return (v >> 17) & 1U; }
    bool is_double_push() const { return (v >> 18) & 1U; }
    bool is_promo() const { return (v >> 19) & 1U; }
    PieceType moved_type() const { return static_cast<PieceType>((v >> 20) & 7); }
    PieceType captured_type() const { return static_cast<PieceType>((v >> 23) & 7); }

    bool is_null() const { return v == 0; }
};

struct ScoredMove {
    Move move;
    int score {0};
};

struct SearchLimits {
    long long wtime_ms {0};
    long long btime_ms {0};
    long long winc_ms {0};
    long long binc_ms {0};
    long long movetime_ms {0};
    long long nodes {0};
    int movestogo {0};
    int depth {0};
    bool infinite {false};
};