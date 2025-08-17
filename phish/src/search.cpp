#include "search.hpp"
#include "eval.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>

Search::Search() {}

void Search::set_board(Board *b) { board = b; }

void Search::clear() {
    tt.clear();
    std::memset(history, 0, sizeof(history));
}

void Search::set_hash_mb(int mb) { tt.resize_mb(static_cast<size_t>(mb)); }

void Search::start(const SearchLimits &lim) {
    limits = lim;
    stopSignal = false;
    if (worker.joinable()) worker.join();
    worker = std::thread([this]{ think(); });
}

void Search::stop() {
    stopSignal = true;
    if (worker.joinable()) worker.join();
}

bool Search::time_up() const {
    if (limits.infinite || limits.movetime_ms > 0) return false;
    long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
    return elapsed >= timeBudgetMs;
}

void Search::pick_move(std::vector<ScoredMove> &moves, size_t idx) const {
    size_t best = idx;
    for (size_t i = idx + 1; i < moves.size(); ++i) if (moves[i].score > moves[best].score) best = i;
    if (best != idx) std::swap(moves[idx], moves[best]);
}

void Search::think() {
    // Time management: simple
    startTime = std::chrono::steady_clock::now();
    Color stm = board->side_to_move();
    long long myTime = (stm == WHITE) ? limits.wtime_ms : limits.btime_ms;
    long long myInc = (stm == WHITE) ? limits.winc_ms : limits.binc_ms;

    if (limits.movetime_ms > 0) timeBudgetMs = limits.movetime_ms;
    else if (myTime > 0) {
        int mtg = limits.movestogo > 0 ? limits.movestogo : 30;
        timeBudgetMs = std::max(10LL, myTime / mtg + myInc / 2);
        timeBudgetMs = std::min(timeBudgetMs, myTime - 50);
    } else timeBudgetMs = 1000;

    Move best = iterative_deepening();

    // Ensure we output a legal move
    if (!best.is_null()) {
        std::cout << "bestmove " << board->move_to_uci(best) << "\n" << std::flush;
    } else {
        std::vector<ScoredMove> legal; board->generate_legal_moves(legal);
        if (!legal.empty()) std::cout << "bestmove " << board->move_to_uci(legal[0].move) << "\n" << std::flush;
        else std::cout << "bestmove 0000\n" << std::flush;
    }
}

Move Search::iterative_deepening() {
    Move bestMove;
    int bestScore = -INF;
    const int maxDepth = limits.depth > 0 ? limits.depth : 64;
    Move pv[128];
    int pvLen = 0;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        int alpha = -INF, beta = INF;
        int score = negamax(depth, alpha, beta, 0, pv, pvLen);
        if (stopSignal) break;
        if (score > bestScore && pvLen > 0) {
            bestScore = score;
            bestMove = pv[0];
        }
        // UCI info
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "info depth " << depth << " score cp " << score << " time " << elapsed << " pv ";
        for (int i = 0; i < pvLen; ++i) std::cout << board->move_to_uci(pv[i]) << ' ';
        std::cout << '\n' << std::flush;
        if (time_up()) break;
    }
    return bestMove;
}

static inline int mvv_lva(PieceType cap, PieceType att) {
    static const int victim[7] = { 100, 320, 330, 500, 900, 20000, 0 };
    static const int attacker[7] = { 1, 2, 3, 4, 5, 6, 0 };
    return victim[cap] * 100 - attacker[att];
}

int Search::negamax(int depth, int alpha, int beta, int ply, Move *pv, int &pvLen) {
    if (stopSignal || time_up()) return 0;

    TTEntry tte; bool hasTT = tt.probe(board->key(), tte);
    if (hasTT && tte.depth >= depth) {
        int ttScore = tte.score;
        if (tte.bound == Bound::EXACT) return ttScore;
        if (tte.bound == Bound::LOWER && ttScore > alpha) alpha = ttScore;
        else if (tte.bound == Bound::UPPER && ttScore < beta) beta = ttScore;
        if (alpha >= beta) return ttScore;
    }

    if (depth <= 0) {
        pvLen = 0;
        return qsearch(alpha, beta, ply);
    }

    // Checkmate/stalemate/draw detection
    if (board->is_draw()) return DRAW_SCORE;
    std::vector<ScoredMove> moves;
    board->generate_legal_moves(moves);
    if (moves.empty()) {
        if (board->side_to_move() == WHITE ? board->square_attacked(lsb(board->piece_bb(WHITE, KING)), BLACK)
                                           : board->square_attacked(lsb(board->piece_bb(BLACK, KING)), WHITE)) {
            return -CHECKMATE + ply; // mate in ply
        }
        return DRAW_SCORE;
    }

    // Static eval for TT and move ordering
    int staticEval = evaluate(*board);

    // Move ordering
    for (auto &sm : moves) {
        Move m = sm.move;
        int score = 0;
        if (m.is_capture()) score = 100000 + mvv_lva(m.captured_type(), m.moved_type());
        else score = history[board->side_to_move()][m.from()][m.to()];
        sm.score = score;
    }
    if (hasTT) {
        Move ttMove = Move(tte.bestMove); // only partial info, but prefer it highly
        for (auto &sm : moves) {
            if (sm.move.from() == ttMove.from() && sm.move.to() == ttMove.to()) sm.score += 200000;
        }
    }
    std::sort(moves.begin(), moves.end(), [](const ScoredMove &a, const ScoredMove &b){ return a.score > b.score; });

    Move bestMove;
    int bestScore = -INF;
    int originalAlpha = alpha;
    int localPvLen = 0;

    for (size_t i = 0; i < moves.size(); ++i) {
        Move m = moves[i].move;
        if (!board->make_move(m)) continue;

        Move childPv[128]; int childPvLen = 0;
        int score;
        if (i == 0) score = -negamax(depth - 1, -beta, -alpha, ply + 1, childPv, childPvLen);
        else {
            // Late Move Reduction (simple)
            int reduction = (depth >= 3 && !m.is_capture()) ? 1 : 0;
            score = -negamax(depth - 1 - reduction, -alpha - 1, -alpha, ply + 1, childPv, childPvLen);
            if (score > alpha) score = -negamax(depth - 1, -beta, -alpha, ply + 1, childPv, childPvLen);
        }
        board->undo_move();

        if (stopSignal) break;

        if (score > bestScore) {
            bestScore = score; bestMove = m;
            if (score > alpha) {
                alpha = score;
                // Build PV
                pv[0] = m; for (int k = 0; k < childPvLen; ++k) pv[k + 1] = childPv[k];
                pvLen = childPvLen + 1;
            }
            if (alpha >= beta) {
                // Update history for quiet moves causing beta-cutoff
                if (!m.is_capture()) history[board->side_to_move()][m.from()][m.to()] += depth * depth;
                break;
            }
        }
    }

    Bound bound = Bound::EXACT;
    if (bestScore <= originalAlpha) bound = Bound::UPPER;
    else if (bestScore >= beta) bound = Bound::LOWER;
    tt.store(board->key(), depth, bestScore, staticEval, bound, bestMove);
    return bestScore;
}

int Search::qsearch(int alpha, int beta, int ply) {
    int standPat = evaluate(*board);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    std::vector<ScoredMove> moves;
    board->generate_pseudo_legal_moves(moves);

    // Only consider captures, promotions, and checks would be better but we'll do captures + promotions
    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const ScoredMove &sm){
        const Move &m = sm.move; return !(m.is_capture() || m.is_promo());
    }), moves.end());

    for (auto &sm : moves) {
        Move m = sm.move;
        if (!board->make_move(m)) continue;
        int score = -qsearch(-beta, -alpha, ply + 1);
        board->undo_move();
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}