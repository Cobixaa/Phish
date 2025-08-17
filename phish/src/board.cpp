#include "board.hpp"
#include <sstream>
#include <cassert>
#include <algorithm>

static inline bool is_between(int f, int l, int x) { return x >= f && x <= l; }

static Bitboard KNIGHT_ATK[64];
static Bitboard KING_ATK[64];
static bool PREPARED = false;

static void prepare_attack_masks() {
    if (PREPARED) return;
    for (int s = 0; s < 64; ++s) {
        int f = file_of(s), r = rank_of(s);
        Bitboard k = 0;
        auto add = [&](int df, int dr) {
            int nf = f + df, nr = r + dr; if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) k |= bit(make_square(nf, nr)); };
        add(1, 2); add(2, 1); add(-1, 2); add(-2, 1); add(1, -2); add(2, -1); add(-1, -2); add(-2, -1);
        KNIGHT_ATK[s] = k;
        k = 0;
        for (int df = -1; df <= 1; ++df) for (int dr = -1; dr <= 1; ++dr) if (df || dr) {
            int nf = f + df, nr = r + dr; if (nf >= 0 && nf < 8 && nr >= 0 && nr < 8) k |= bit(make_square(nf, nr));
        }
        KING_ATK[s] = k;
    }
    PREPARED = true;
}

Board::Board() {
    zobKeys = init_zobrist();
    clear();
}

void Board::clear() {
    board.fill(PIECE_EMPTY);
    for (int c = 0; c < 2; ++c) {
        for (int p = 0; p < 6; ++p) pieceBB[c][p] = 0ULL;
        occByColor[c] = 0ULL;
    }
    occAll = 0ULL;
    stm = WHITE;
    castlingRights = 0;
    epSquare = -1;
    halfmoveClock = 0;
    fullmoveNumber = 1;
    zobrist = 0ULL;
    states.clear();
    prepare_attack_masks();
}

void Board::set_startpos() {
    clear();
    // Place pieces
    auto place_backrank = [&](Color c, int rank) {
        int pieces[8] = { make_piece(c, ROOK), make_piece(c, KNIGHT), make_piece(c, BISHOP), make_piece(c, QUEEN), make_piece(c, KING), make_piece(c, BISHOP), make_piece(c, KNIGHT), make_piece(c, ROOK) };
        for (int f = 0; f < 8; ++f) put_piece(make_square(f, rank), pieces[f]);
    };
    auto place_pawns = [&](Color c, int rank) {
        for (int f = 0; f < 8; ++f) put_piece(make_square(f, rank), make_piece(c, PAWN));
    };
    place_backrank(WHITE, 0);
    place_pawns(WHITE, 1);
    place_backrank(BLACK, 7);
    place_pawns(BLACK, 6);
    stm = WHITE;
    castlingRights = WK | WQ | BK | BQ;
    epSquare = -1;
    halfmoveClock = 0;
    fullmoveNumber = 1;

    zobrist = 0ULL;
    for (int s = 0; s < 64; ++s) if (board[s] != PIECE_EMPTY) {
        int pc = board[s];
        zobrist ^= zobKeys.piece[piece_color(pc)][piece_type(pc)][s];
    }
    zobrist ^= zobKeys.castling[castlingRights];
}

bool Board::set_fen(const std::string &fen) {
    clear();
    std::istringstream iss(fen);
    std::string boardPart, stmPart, castlePart, epPart; int half, full;
    if (!(iss >> boardPart >> stmPart >> castlePart >> epPart >> half >> full)) {
        // Some FENs might omit clocks, try partial
        iss.clear(); iss.str(fen);
        if (!(iss >> boardPart >> stmPart >> castlePart >> epPart)) return false;
        half = 0; full = 1;
    }
    int sq = 56; // A8
    for (char c : boardPart) {
        if (c == '/') { sq -= 16; continue; }
        if (c >= '1' && c <= '8') { sq += (c - '0'); continue; }
        Color col = std::isupper(static_cast<unsigned char>(c)) ? WHITE : BLACK;
        char lc = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        PieceType pt = NO_PIECE_TYPE;
        if (lc == 'p') pt = PAWN; else if (lc == 'n') pt = KNIGHT; else if (lc == 'b') pt = BISHOP; else if (lc == 'r') pt = ROOK; else if (lc == 'q') pt = QUEEN; else if (lc == 'k') pt = KING;
        if (pt == NO_PIECE_TYPE) return false;
        put_piece(sq, make_piece(col, pt));
        ++sq;
    }
    stm = (stmPart == "w") ? WHITE : BLACK;
    castlingRights = 0;
    if (castlePart.find('K') != std::string::npos) castlingRights |= WK;
    if (castlePart.find('Q') != std::string::npos) castlingRights |= WQ;
    if (castlePart.find('k') != std::string::npos) castlingRights |= BK;
    if (castlePart.find('q') != std::string::npos) castlingRights |= BQ;

    if (epPart != "-") {
        int file = epPart[0] - 'a'; int rank = epPart[1] - '1';
        epSquare = make_square(file, rank);
    } else epSquare = -1;

    halfmoveClock = half;
    fullmoveNumber = full;

    zobrist = 0ULL;
    for (int s2 = 0; s2 < 64; ++s2) if (board[s2] != PIECE_EMPTY) {
        int pc = board[s2];
        zobrist ^= zobKeys.piece[piece_color(pc)][piece_type(pc)][s2];
    }
    zobrist ^= zobKeys.castling[castlingRights];
    if (stm == BLACK) zobrist ^= zobKeys.side;
    if (epSquare != -1) zobrist ^= zobKeys.epFile[file_of(epSquare)];
    return true;
}

void Board::put_piece(int sq, int piece) {
    board[sq] = piece;
    if (piece != PIECE_EMPTY) {
        pieceBB[piece_color(piece)][piece_type(piece)] |= bit(sq);
        occByColor[piece_color(piece)] |= bit(sq);
        occAll |= bit(sq);
    }
}

void Board::remove_piece(int sq) {
    int piece = board[sq];
    if (piece == PIECE_EMPTY) return;
    pieceBB[piece_color(piece)][piece_type(piece)] &= ~bit(sq);
    occByColor[piece_color(piece)] &= ~bit(sq);
    occAll &= ~bit(sq);
    board[sq] = PIECE_EMPTY;
}

void Board::move_piece(int from, int to) {
    int piece = board[from];
    remove_piece(from);
    put_piece(to, piece);
}

bool Board::in_check(Color side) const {
    // Find king square
    Bitboard kb = pieceBB[side][KING];
    if (!kb) return false;
    int ks = lsb(kb);
    return square_attacked(ks, opposite(side));
}

bool Board::is_draw() const {
    if (halfmoveClock >= 100) return true; // 50-move
    // Simple threefold by counting current key in states + current position
    int count = 1; // current
    for (const auto &st : states) if (st.zobristKey == zobrist) ++count;
    return count >= 3;
}

Bitboard Board::attacks_from_bishop(int sq, Bitboard occ) const {
    Bitboard attacks = 0ULL;
    int f = file_of(sq), r = rank_of(sq);
    // NE
    for (int nf = f + 1, nr = r + 1; nf < 8 && nr < 8; ++nf, ++nr) { int s2 = make_square(nf, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // NW
    for (int nf = f - 1, nr = r + 1; nf >= 0 && nr < 8; --nf, ++nr) { int s2 = make_square(nf, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // SE
    for (int nf = f + 1, nr = r - 1; nf < 8 && nr >= 0; ++nf, --nr) { int s2 = make_square(nf, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // SW
    for (int nf = f - 1, nr = r - 1; nf >= 0 && nr >= 0; --nf, --nr) { int s2 = make_square(nf, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    return attacks;
}

Bitboard Board::attacks_from_rook(int sq, Bitboard occ) const {
    Bitboard attacks = 0ULL;
    int f = file_of(sq), r = rank_of(sq);
    // E
    for (int nf = f + 1; nf < 8; ++nf) { int s2 = make_square(nf, r); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // W
    for (int nf = f - 1; nf >= 0; --nf) { int s2 = make_square(nf, r); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // N
    for (int nr = r + 1; nr < 8; ++nr) { int s2 = make_square(f, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    // S
    for (int nr = r - 1; nr >= 0; --nr) { int s2 = make_square(f, nr); attacks |= bit(s2); if (occ & bit(s2)) break; }
    return attacks;
}

bool Board::square_attacked(int sq, Color by) const {
    // Pawns
    if (by == WHITE) {
        Bitboard pawns = pieceBB[WHITE][PAWN];
        Bitboard atk = (pawns & ~file_mask(7)) << 9; // a-h: careful
        atk |= (pawns & ~file_mask(0)) << 7;
        if (atk & bit(sq)) return true;
    } else {
        Bitboard pawns = pieceBB[BLACK][PAWN];
        Bitboard atk = (pawns & ~file_mask(0)) >> 9;
        atk |= (pawns & ~file_mask(7)) >> 7;
        if (atk & bit(sq)) return true;
    }
    // Knights
    if (KNIGHT_ATK[sq] & pieceBB[by][KNIGHT]) return true;
    // Bishops/Queens
    Bitboard bishops = pieceBB[by][BISHOP] | pieceBB[by][QUEEN];
    Bitboard rooks = pieceBB[by][ROOK] | pieceBB[by][QUEEN];
    if (attacks_from_bishop(sq, occAll) & bishops) return true;
    if (attacks_from_rook(sq, occAll) & rooks) return true;
    // King
    if (KING_ATK[sq] & pieceBB[by][KING]) return true;
    return false;
}

void Board::generate_pseudo_legal_moves(std::vector<ScoredMove> &out) const {
    Color side = stm;
    gen_pawn_moves(side, out);
    gen_knight_moves(side, out);
    gen_bishop_moves(side, out);
    gen_rook_moves(side, out);
    gen_queen_moves(side, out);
    gen_king_moves(side, out);
}

void Board::generate_legal_moves(std::vector<ScoredMove> &out) const {
    std::vector<ScoredMove> pseudo;
    pseudo.reserve(128);
    generate_pseudo_legal_moves(pseudo);

    Board copy = *this; // copy to make/undo cheaply; since Board is moderate size, it's okay
    for (const auto &sm : pseudo) {
        if (copy.make_move(sm.move)) {
            // If move leaves the opponent to move in check status is fine
            out.push_back(sm);
            copy.undo_move();
        }
    }
}

void Board::gen_pawn_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard pawns = pieceBB[side][PAWN];
    Bitboard us = occByColor[side];
    Bitboard them = occByColor[opposite(side)];
    Bitboard empty = ~occAll;

    int dir = side == WHITE ? 8 : -8;
    int startRank = side == WHITE ? 1 : 6;
    int promoRank = side == WHITE ? 6 : 1; // rank before promotion push

    Bitboard p = pawns;
    while (p) {
        int s = pop_lsb(p);
        int r = rank_of(s);
        int f = file_of(s);
        int one = s + dir;
        if (one >= 0 && one < 64 && (empty & bit(one))) {
            if (r == promoRank) {
                out.push_back({ Move::make(s, one, PAWN, NO_PIECE_TYPE, QUEEN), 0 });
                out.push_back({ Move::make(s, one, PAWN, NO_PIECE_TYPE, ROOK), 0 });
                out.push_back({ Move::make(s, one, PAWN, NO_PIECE_TYPE, BISHOP), 0 });
                out.push_back({ Move::make(s, one, PAWN, NO_PIECE_TYPE, KNIGHT), 0 });
            } else {
                bool dblOk = (r == startRank);
                out.push_back({ Move::make(s, one, PAWN), 0 });
                int two = one + dir;
                if (dblOk && (empty & bit(two))) {
                    out.push_back({ Move::make(s, two, PAWN, NO_PIECE_TYPE, NO_PIECE_TYPE, false, false, false, true), 0 });
                }
            }
        }
        // Captures
        int dl = side == WHITE ? 7 : -9;
        int dr = side == WHITE ? 9 : -7;
        if (f > 0) {
            int to = s + dl;
            if (to >= 0 && to < 64 && (them & bit(to))) {
                if (r == promoRank) {
                    int capType = piece_type(board[to]);
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), QUEEN, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), ROOK, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), BISHOP, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), KNIGHT, true), 0 });
                } else {
                    int capType = piece_type(board[to]);
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), NO_PIECE_TYPE, true), 0 });
                }
            }
        }
        if (f < 7) {
            int to = s + dr;
            if (to >= 0 && to < 64 && (them & bit(to))) {
                if (r == promoRank) {
                    int capType = piece_type(board[to]);
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), QUEEN, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), ROOK, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), BISHOP, true), 0 });
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), KNIGHT, true), 0 });
                } else {
                    int capType = piece_type(board[to]);
                    out.push_back({ Move::make(s, to, PAWN, static_cast<PieceType>(capType), NO_PIECE_TYPE, true), 0 });
                }
            }
        }
        // En passant
        if (epSquare != -1) {
            int ep = epSquare;
            if (side == WHITE) {
                if (r == 4 && (ep == s + 7 || ep == s + 9)) {
                    if ((ep == s + 7 && f > 0) || (ep == s + 9 && f < 7))
                        out.push_back({ Move::make(s, ep, PAWN, PAWN, NO_PIECE_TYPE, true, true), 0 });
                }
            } else {
                if (r == 3 && (ep == s - 7 || ep == s - 9)) {
                    if ((ep == s - 7 && f < 7) || (ep == s - 9 && f > 0))
                        out.push_back({ Move::make(s, ep, PAWN, PAWN, NO_PIECE_TYPE, true, true), 0 });
                }
            }
        }
    }
}

void Board::gen_knight_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard knights = pieceBB[side][KNIGHT];
    Bitboard own = occByColor[side];
    while (knights) {
        int s = pop_lsb(knights);
        Bitboard atk = KNIGHT_ATK[s] & ~own;
        Bitboard bb = atk;
        while (bb) {
            int to = pop_lsb(bb);
            bool cap = (occByColor[opposite(side)] & bit(to)) != 0ULL;
            PieceType capType = cap ? piece_type(board[to]) : NO_PIECE_TYPE;
            out.push_back({ Move::make(s, to, KNIGHT, capType, NO_PIECE_TYPE, cap), 0 });
        }
    }
}

void Board::gen_bishop_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard bishops = pieceBB[side][BISHOP];
    Bitboard own = occByColor[side];
    while (bishops) {
        int s = pop_lsb(bishops);
        Bitboard atk = attacks_from_bishop(s, occAll) & ~own;
        Bitboard bb = atk;
        while (bb) {
            int to = pop_lsb(bb);
            bool cap = (occByColor[opposite(side)] & bit(to)) != 0ULL;
            PieceType capType = cap ? piece_type(board[to]) : NO_PIECE_TYPE;
            out.push_back({ Move::make(s, to, BISHOP, capType, NO_PIECE_TYPE, cap), 0 });
        }
    }
}

void Board::gen_rook_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard rooks = pieceBB[side][ROOK];
    Bitboard own = occByColor[side];
    while (rooks) {
        int s = pop_lsb(rooks);
        Bitboard atk = attacks_from_rook(s, occAll) & ~own;
        Bitboard bb = atk;
        while (bb) {
            int to = pop_lsb(bb);
            bool cap = (occByColor[opposite(side)] & bit(to)) != 0ULL;
            PieceType capType = cap ? piece_type(board[to]) : NO_PIECE_TYPE;
            out.push_back({ Move::make(s, to, ROOK, capType, NO_PIECE_TYPE, cap), 0 });
        }
    }
}

void Board::gen_queen_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard queens = pieceBB[side][QUEEN];
    Bitboard own = occByColor[side];
    while (queens) {
        int s = pop_lsb(queens);
        Bitboard atk = (attacks_from_bishop(s, occAll) | attacks_from_rook(s, occAll)) & ~own;
        Bitboard bb = atk;
        while (bb) {
            int to = pop_lsb(bb);
            bool cap = (occByColor[opposite(side)] & bit(to)) != 0ULL;
            PieceType capType = cap ? piece_type(board[to]) : NO_PIECE_TYPE;
            out.push_back({ Move::make(s, to, QUEEN, capType, NO_PIECE_TYPE, cap), 0 });
        }
    }
}

void Board::gen_king_moves(Color side, std::vector<ScoredMove> &out) const {
    Bitboard kings = pieceBB[side][KING];
    if (!kings) return;
    int s = lsb(kings);
    Bitboard own = occByColor[side];
    Bitboard atk = KING_ATK[s] & ~own;
    Bitboard bb = atk;
    while (bb) {
        int to = pop_lsb(bb);
        bool cap = (occByColor[opposite(side)] & bit(to)) != 0ULL;
        PieceType capType = cap ? piece_type(board[to]) : NO_PIECE_TYPE;
        out.push_back({ Move::make(s, to, KING, capType, NO_PIECE_TYPE, cap), 0 });
    }
    // Castling
    if (!in_check(side)) {
        if (side == WHITE) {
            if ((castlingRights & WK) && !(occAll & (bit(5) | bit(6))) &&
                !square_attacked(5, BLACK) && !square_attacked(6, BLACK)) {
                out.push_back({ Move::make(4, 6, KING, NO_PIECE_TYPE, NO_PIECE_TYPE, false, false, true), 0 });
            }
            if ((castlingRights & WQ) && !(occAll & (bit(1) | bit(2) | bit(3))) &&
                !square_attacked(3, BLACK) && !square_attacked(2, BLACK)) {
                out.push_back({ Move::make(4, 2, KING, NO_PIECE_TYPE, NO_PIECE_TYPE, false, false, true), 0 });
            }
        } else {
            if ((castlingRights & BK) && !(occAll & (bit(61) | bit(62))) &&
                !square_attacked(61, WHITE) && !square_attacked(62, WHITE)) {
                out.push_back({ Move::make(60, 62, KING, NO_PIECE_TYPE, NO_PIECE_TYPE, false, false, true), 0 });
            }
            if ((castlingRights & BQ) && !(occAll & (bit(57) | bit(58) | bit(59))) &&
                !square_attacked(59, WHITE) && !square_attacked(58, WHITE)) {
                out.push_back({ Move::make(60, 58, KING, NO_PIECE_TYPE, NO_PIECE_TYPE, false, false, true), 0 });
            }
        }
    }
}

void Board::update_zobrist_for_piece(int sq, int piece) {
    if (piece == PIECE_EMPTY) return;
    zobrist ^= zobKeys.piece[piece_color(piece)][piece_type(piece)][sq];
}

bool Board::make_move(const Move &m) {
    BoardState st{};
    st.zobristKey = zobrist;
    st.castlingRights = castlingRights;
    st.epSquare = epSquare;
    st.halfmoveClock = halfmoveClock;
    st.move = m;
    st.capturedPiece = PIECE_EMPTY;

    // Push state early so undo_move() can always revert
    states.push_back(st);

    int from = m.from();
    int to = m.to();
    int moving = board[from];
    Color us = stm;
    Color them = opposite(us);

    // Remove EP/Zob from previous
    if (epSquare != -1) zobrist ^= zobKeys.epFile[file_of(epSquare)];
    zobrist ^= zobKeys.castling[castlingRights];

    // Halfmove clock
    if (piece_type(moving) == PAWN || m.is_capture()) halfmoveClock = 0; else ++halfmoveClock;

    // Handle capture
    if (m.is_capture()) {
        if (m.is_ep()) {
            int capSq = us == WHITE ? to - 8 : to + 8;
            states.back().capturedPiece = board[capSq];
            remove_piece(capSq);
            update_zobrist_for_piece(capSq, states.back().capturedPiece);
        } else {
            states.back().capturedPiece = board[to];
            remove_piece(to);
            update_zobrist_for_piece(to, states.back().capturedPiece);
        }
    }

    // Move piece
    update_zobrist_for_piece(from, moving);
    remove_piece(from);

    int placedPiece = moving;
    // Promotions
    if (m.is_promo()) {
        placedPiece = make_piece(us, m.promo());
    }

    put_piece(to, placedPiece);
    update_zobrist_for_piece(to, placedPiece);

    // Castling rook moves
    if (m.is_castle()) {
        if (us == WHITE) {
            if (to == 6) { // O-O
                remove_piece(7); update_zobrist_for_piece(7, make_piece(WHITE, ROOK));
                put_piece(5, make_piece(WHITE, ROOK)); update_zobrist_for_piece(5, make_piece(WHITE, ROOK));
            } else if (to == 2) { // O-O-O
                remove_piece(0); update_zobrist_for_piece(0, make_piece(WHITE, ROOK));
                put_piece(3, make_piece(WHITE, ROOK)); update_zobrist_for_piece(3, make_piece(WHITE, ROOK));
            }
        } else {
            if (to == 62) { // O-O
                remove_piece(63); update_zobrist_for_piece(63, make_piece(BLACK, ROOK));
                put_piece(61, make_piece(BLACK, ROOK)); update_zobrist_for_piece(61, make_piece(BLACK, ROOK));
            } else if (to == 58) { // O-O-O
                remove_piece(56); update_zobrist_for_piece(56, make_piece(BLACK, ROOK));
                put_piece(59, make_piece(BLACK, ROOK)); update_zobrist_for_piece(59, make_piece(BLACK, ROOK));
            }
        }
    }

    // Update castling rights if king/rook moved or rook captured
    auto clear_rook_right = [&](int sq) {
        if (sq == 0) castlingRights &= ~WQ;
        else if (sq == 7) castlingRights &= ~WK;
        else if (sq == 56) castlingRights &= ~BQ;
        else if (sq == 63) castlingRights &= ~BK;
    };
    if (piece_type(moving) == KING) {
        if (us == WHITE) castlingRights &= ~(WK | WQ); else castlingRights &= ~(BK | BQ);
    }
    if (piece_type(moving) == ROOK) clear_rook_right(from);
    if (states.back().capturedPiece != PIECE_EMPTY && piece_type(states.back().capturedPiece) == ROOK) clear_rook_right(to);

    // EP square update
    if (m.is_double_push()) {
        epSquare = us == WHITE ? (to - 8) : (to + 8);
    } else epSquare = -1;

    // Zobrist for castle/ep/side
    zobrist ^= zobKeys.castling[castlingRights];
    if (epSquare != -1) zobrist ^= zobKeys.epFile[file_of(epSquare)];

    stm = them;
    zobrist ^= zobKeys.side;

    // Illegal if our king is in check after move
    if (in_check(opposite(stm))) {
        undo_move();
        return false;
    }

    if (stm == WHITE) ++fullmoveNumber;
    return true;
}

void Board::undo_move() {
    if (states.empty()) return;
    BoardState st = states.back();
    states.pop_back();

    Move m = st.move;
    int from = m.from();
    int to = m.to();
    Color them = stm; // side who just moved
    Color us = opposite(them);

    // Revert side and zobrist
    stm = us;
    zobrist = st.zobristKey;
    castlingRights = st.castlingRights;
    epSquare = st.epSquare;
    halfmoveClock = st.halfmoveClock;

    // Move piece back
    int movedPieceAfter = board[to];
    remove_piece(to);
    if (m.is_promo()) {
        // Was promoted, restore pawn
        put_piece(from, make_piece(us, PAWN));
    } else {
        put_piece(from, movedPieceAfter);
    }

    // Restore captured piece
    if (m.is_capture()) {
        if (m.is_ep()) {
            int capSq = us == WHITE ? to - 8 : to + 8;
            put_piece(capSq, st.capturedPiece);
        } else {
            put_piece(to, st.capturedPiece);
        }
    }

    // Undo castling rook move
    if (m.is_castle()) {
        if (us == WHITE) {
            if (to == 6) { remove_piece(5); put_piece(7, make_piece(WHITE, ROOK)); }
            else if (to == 2) { remove_piece(3); put_piece(0, make_piece(WHITE, ROOK)); }
        } else {
            if (to == 62) { remove_piece(61); put_piece(63, make_piece(BLACK, ROOK)); }
            else if (to == 58) { remove_piece(59); put_piece(56, make_piece(BLACK, ROOK)); }
        }
    }
}

Move Board::parse_uci_move(const std::string &uciMove) const {
    if (uciMove.size() < 4) return Move();
    int from = (uciMove[0] - 'a') + 8 * (uciMove[1] - '1');
    int to   = (uciMove[2] - 'a') + 8 * (uciMove[3] - '1');
    PieceType promo = NO_PIECE_TYPE;
    if (uciMove.size() >= 5) {
        char pc = uciMove[4];
        if (pc == 'q' || pc == 'Q') promo = QUEEN;
        else if (pc == 'r' || pc == 'R') promo = ROOK;
        else if (pc == 'b' || pc == 'B') promo = BISHOP;
        else if (pc == 'n' || pc == 'N') promo = KNIGHT;
    }

    // Find among legal moves
    std::vector<ScoredMove> legal;
    generate_legal_moves(legal);
    for (const auto &sm : legal) {
        if (sm.move.from() == from && sm.move.to() == to) {
            if (!sm.move.is_promo() || sm.move.promo() == promo) return sm.move;
        }
    }
    return Move();
}

std::string Board::move_to_uci(const Move &m) const {
    std::string s;
    s.resize(4);
    int from = m.from(); int to = m.to();
    s[0] = 'a' + file_of(from);
    s[1] = '1' + rank_of(from);
    s[2] = 'a' + file_of(to);
    s[3] = '1' + rank_of(to);
    if (m.is_promo()) {
        char pc = 'q';
        switch (m.promo()) { case QUEEN: pc = 'q'; break; case ROOK: pc = 'r'; break; case BISHOP: pc = 'b'; break; case KNIGHT: pc = 'n'; break; default: break; }
        s.push_back(pc);
    }
    return s;
}