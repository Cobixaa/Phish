#include "uci.hpp"
#include <iostream>

int main() {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    Uci uci;
    uci.loop();
    return 0;
}