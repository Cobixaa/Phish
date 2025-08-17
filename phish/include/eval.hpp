#pragma once
#include "types.hpp"
#include "board.hpp"

struct EvalResult {
    int score;
};

int evaluate(const Board &board);