#pragma once
#include <array>
#include <vector>
#include <string>
#include <cstdint>
#include "types.hpp"
#include "zobrist.hpp"

struct BoardState {
    uint64_t zobristKey;
    int castlingRights;
    int epSquare; // -1 if none
    int halfmoveClock;
    Move move; // move that led to this state
    int capturedPiece; // PIECE_EMPTY or piece code
    bool wasNull {false};
};

class Board {
public:
    Board();

    void set_startpos();
    bool set_fen(const std::string &fen);

    // Move handling
    void generate_legal_moves(std::vector<ScoredMove> &out) const;
    void generate_pseudo_legal_moves(std::vector<ScoredMove> &out) const;
    bool make_move(const Move &m);
    void undo_move();

    // Null move
    bool make_null_move();
    void undo_null_move();

    // Parsing/formatting
    Move parse_uci_move(const std::string &uciMove) const;
    std::string move_to_uci(const Move &m) const;

    // Attacks
    bool square_attacked(int sq, Color by) const;

    // Accessors
    inline Color side_to_move() const { return stm; }
    inline uint64_t key() const { return zobrist; }
    inline int piece_on(int sq) const { return board[sq]; }

    // For evaluation
    inline Bitboard occupancy() const { return occAll; }
    inline Bitboard occupancy(Color c) const { return occByColor[c]; }
    inline Bitboard piece_bb(Color c, PieceType pt) const { return pieceBB[c][pt]; }
    inline int castling_rights() const { return castlingRights; }
    inline int ep_square() const { return epSquare; }

    // Utilities
    void clear();
    bool is_draw() const; // threefold or 50-move
    inline bool in_check_now() const { return in_check(stm); }

private:
    // Representation
    std::array<int, 64> board; // piece code or PIECE_EMPTY
    Bitboard pieceBB[2][6];
    Bitboard occByColor[2];
    Bitboard occAll;
    Color stm;
    int castlingRights; // bits: WK|WQ|BK|BQ
    int epSquare; // -1 none
    int halfmoveClock;
    int fullmoveNumber;

    // Zobrist
    ZobristKeys zobKeys;
    uint64_t zobrist;

    // State stack
    std::vector<BoardState> states;

    // Helpers
    void put_piece(int sq, int piece);
    void remove_piece(int sq);
    void move_piece(int from, int to);
    void update_zobrist_for_piece(int sq, int piece);

    void gen_pawn_moves(Color side, std::vector<ScoredMove> &out) const;
    void gen_knight_moves(Color side, std::vector<ScoredMove> &out) const;
    void gen_bishop_moves(Color side, std::vector<ScoredMove> &out) const;
    void gen_rook_moves(Color side, std::vector<ScoredMove> &out) const;
    void gen_queen_moves(Color side, std::vector<ScoredMove> &out) const;
    void gen_king_moves(Color side, std::vector<ScoredMove> &out) const;

    Bitboard attacks_from_bishop(int sq, Bitboard occ) const;
    Bitboard attacks_from_rook(int sq, Bitboard occ) const;

    bool in_check(Color side) const;
};