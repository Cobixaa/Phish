#include "search.hpp"
#include "eval.hpp"
#include <iostream>
#include <algorithm>
#include <cstring>
#include <limits>

Search::Search() {}

void Search::set_board(Board *b) { board = b; }

void Search::clear() {
    tt.clear();
    std::memset(history, 0, sizeof(history));
    std::memset(killers, 0, sizeof(killers));
    nodes = 0;
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

    int aspirationWindow = 30;

    for (int depth = 1; depth <= maxDepth; ++depth) {
        int alpha = -INF, beta = INF;
        if (depth > 3 && bestScore > -INF/2) { alpha = bestScore - aspirationWindow; beta = bestScore + aspirationWindow; }
        int score = negamax(depth, alpha, beta, 0, pv, pvLen);
        if ((score <= alpha || score >= beta) && !stopSignal) {
            alpha = -INF; beta = INF; // fail-low/high, re-search full window
            score = negamax(depth, alpha, beta, 0, pv, pvLen);
            aspirationWindow = std::min(500, aspirationWindow * 2);
        }
        if (stopSignal) break;
        if (score > bestScore && pvLen > 0) { bestScore = score; bestMove = pv[0]; }
        long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - startTime).count();
        std::cout << "info depth " << depth << " score cp " << score << " time " << elapsed << " nodes " << nodes.load() << " pv ";
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

int Search::negamax(int depth, int alpha, int beta, int ply, Move *pv, int &pvLen, bool allowNull) {
    if (stopSignal || time_up()) return 0;
    nodes++;

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

    // Static eval for pruning and move ordering
    int staticEval = evaluate(*board);

    // Razoring: if static eval is well below alpha near the horizon, try qsearch
    bool inCheck = board->in_check_now();
    if (!inCheck && depth <= 2 && !hasTT) {
        static const int RAZOR_MARGIN[3] = {0, 200, 500};
        if (staticEval + RAZOR_MARGIN[depth] <= alpha) {
            int r = qsearch(alpha, beta, ply);
            if (r <= alpha) return r;
        }
    }

    // Null-move pruning (avoid in check and near mate scores)
    if (allowNull && depth >= 3 && !inCheck && staticEval >= beta) {
        if (board->make_null_move()) {
            int R = 2 + (depth / 4); // dynamic reduction
            Move dummyPv[2]; int dummyLen = 0;
            int score = -negamax(depth - 1 - R, -beta, -beta + 1, ply + 1, dummyPv, dummyLen, false);
            board->undo_move();
            if (score >= beta) return score;
        }
    }

    std::vector<ScoredMove> moves;
    board->generate_legal_moves_nc(moves);
    if (moves.empty()) {
        if (board->side_to_move() == WHITE ? board->square_attacked(lsb(board->piece_bb(WHITE, KING)), BLACK)
                                           : board->square_attacked(lsb(board->piece_bb(BLACK, KING)), WHITE)) {
            return -CHECKMATE + ply; // mate in ply
        }
        return DRAW_SCORE;
    }

    // Move ordering: TT, captures MVV-LVA, killers, history
    for (auto &sm : moves) {
        Move m = sm.move;
        int score = 0;
        if (hasTT) {
            Move ttMove = Move(tte.bestMove);
            if (m.from() == ttMove.from() && m.to() == ttMove.to()) score += 300000;
        }
        if (m.is_capture()) score += 100000 + mvv_lva(m.captured_type(), m.moved_type());
        if (!m.is_capture()) {
            if (killers[ply][0].v && m.v == killers[ply][0].v) score += 90000;
            else if (killers[ply][1].v && m.v == killers[ply][1].v) score += 80000;
            score += history[board->side_to_move()][m.from()][m.to()];
        }
        sm.score = score;
    }
    std::sort(moves.begin(), moves.end(), [](const ScoredMove &a, const ScoredMove &b){ return a.score > b.score; });

    Move bestMove;
    int bestScore = -INF;
    int originalAlpha = alpha;

    int moveCount = 0;
    bool inPV = (beta - alpha) > 1;

    for (size_t i = 0; i < moves.size(); ++i) {
        Move m = moves[i].move;
        if (!board->make_move(m)) continue;
        ++moveCount;

        // Determine if the move gives check (for extension and pruning safety)
        bool givesCheck = board->in_check_now();

        // Futility pruning: shallow depth, not in check, quiet move, and static eval far below alpha
        if (!inCheck && !givesCheck && !m.is_capture() && depth <= 3 && !inPV) {
            static const int FUT_MARGIN[4] = {0, 150, 300, 500};
            if (staticEval + FUT_MARGIN[depth] <= alpha) {
                board->undo_move();
                continue;
            }
        }

        // Late move pruning for quiet moves far down the list
        if (depth <= 3 && !m.is_capture() && moveCount > 8 + depth && !givesCheck) {
            board->undo_move();
            continue;
        }

        Move childPv[128]; int childPvLen = 0;
        int score;
        int newDepth = depth - 1 + (givesCheck ? 1 : 0);
        if (i == 0) score = -negamax(newDepth, -beta, -alpha, ply + 1, childPv, childPvLen);
        else {
            int reduction = 0;
            if (depth >= 3 && !m.is_capture()) {
                reduction = 1 + (moveCount > 4) + (depth >= 5);
                if (givesCheck) reduction = std::max(0, reduction - 1);
            }
            score = -negamax(newDepth - reduction, -alpha - 1, -alpha, ply + 1, childPv, childPvLen);
            if (score > alpha) score = -negamax(newDepth, -beta, -alpha, ply + 1, childPv, childPvLen);
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
                // Update history and killers on quiet beta cutoff
                if (!m.is_capture()) {
                    history[board->side_to_move()][m.from()][m.to()] += depth * depth;
                    if (killers[ply][0].v != m.v) { killers[ply][1] = killers[ply][0]; killers[ply][0] = m; }
                }
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
    nodes++;
    int standPat = evaluate(*board);
    if (standPat >= beta) return standPat;
    if (standPat > alpha) alpha = standPat;

    std::vector<ScoredMove> moves;
    board->generate_pseudo_legal_moves(moves);

    moves.erase(std::remove_if(moves.begin(), moves.end(), [&](const ScoredMove &sm){
        const Move &m = sm.move; return !(m.is_capture() || m.is_promo());
    }), moves.end());

    // Order captures by MVV-LVA
    for (auto &sm : moves) {
        const Move &m = sm.move;
        sm.score = 100000 + mvv_lva(m.captured_type(), m.moved_type());
        if (m.is_promo()) sm.score += 50000;
    }
    std::sort(moves.begin(), moves.end(), [](const ScoredMove &a, const ScoredMove &b){ return a.score > b.score; });

    for (auto &sm : moves) {
        Move m = sm.move;
        // Delta pruning: skip captures that cannot possibly raise alpha
        static const int V[7] = { 100, 320, 330, 500, 900, 20000, 0 };
        int optimisticGain = V[m.captured_type()] + (m.is_promo() ? (V[QUEEN] - V[PAWN]) : 0);
        if (standPat + optimisticGain + 50 < alpha) continue;

        if (!board->make_move(m)) continue;
        int score = -qsearch(-beta, -alpha, ply + 1);
        board->undo_move();
        if (score >= beta) return score;
        if (score > alpha) alpha = score;
    }
    return alpha;
}