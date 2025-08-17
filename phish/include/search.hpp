#pragma once
#include <atomic>
#include <thread>
#include <vector>
#include <chrono>
#include "types.hpp"
#include "board.hpp"
#include "tt.hpp"

class Search {
public:
    Search();
    void set_board(Board *b);
    void clear();
    void set_hash_mb(int mb);

    void start(const SearchLimits &limits);
    void stop();

private:
    Board *board {nullptr};
    TranspositionTable tt;

    std::atomic<bool> stopSignal {false};
    std::thread worker;

    // History heuristics
    int history[2][64][64]{};

    // Limits
    SearchLimits limits;
    std::chrono::steady_clock::time_point startTime;
    long long timeBudgetMs {0};

    // Main
    void think();
    Move iterative_deepening();
    int negamax(int depth, int alpha, int beta, int ply, Move *pv, int &pvLen);
    int qsearch(int alpha, int beta, int ply);

    // Utils
    bool time_up() const;
    void pick_move(std::vector<ScoredMove> &moves, size_t idx) const;
};