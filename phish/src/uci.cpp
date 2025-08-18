#include "uci.hpp"
#include "board.hpp"
#include "search.hpp"
#include <iostream>
#include <sstream>
#include <vector>
#include <cctype>
#include <charconv>

static std::vector<std::string> split(const std::string &s) {
    std::istringstream iss(s);
    std::vector<std::string> out;
    std::string t;
    while (iss >> t) out.push_back(t);
    return out;
}

static inline bool parse_int_str(const std::string &s, int &out) {
    const char *b = s.data();
    const char *e = b + s.size();
    auto res = std::from_chars(b, e, out);
    return res.ec == std::errc();
}

static inline bool parse_ll_str(const std::string &s, long long &out) {
    const char *b = s.data();
    const char *e = b + s.size();
    auto res = std::from_chars(b, e, out);
    return res.ec == std::errc();
}

void Uci::loop() {
    Board board;
    Search search;
    search.set_board(&board);

    std::string line;
    for (;;) {
        if (!std::getline(std::cin, line)) break;
        if (line.empty()) continue;
        auto tokens = split(line);
        const std::string &cmd = tokens[0];

        if (cmd == "uci") {
            std::cout << "id name phish" << '\n';
            std::cout << "id author Cursor-GPT" << '\n';
            std::cout << "option name Hash type spin default 64 min 1 max 4096" << '\n';
            std::cout << "option name Threads type spin default 1 min 1 max 1" << '\n';
            std::cout << "uciok" << '\n' << std::flush;
        } else if (cmd == "isready") {
            std::cout << "readyok" << '\n' << std::flush;
        } else if (cmd == "ucinewgame") {
            board.set_startpos();
            search.clear();
        } else if (cmd == "setoption") {
            // setoption name Hash value N
            std::string name, value; int v = 0;
            for (size_t i = 1; i + 1 < tokens.size(); ++i) {
                if (tokens[i] == "name") {
                    name.clear();
                    size_t j = i + 1;
                    while (j < tokens.size() && tokens[j] != "value") {
                        if (!name.empty()) name.push_back(' ');
                        name += tokens[j++];
                    }
                    i = j - 1;
                } else if (tokens[i] == "value" && i + 1 < tokens.size()) {
                    value = tokens[++i];
                }
            }
            if (name == "Hash") {
                v = 64;
                parse_int_str(value, v);
                search.set_hash_mb(v);
            }
        } else if (cmd == "position") {
            // position [fen <fenstring> | startpos ]  moves <move1> ....
            size_t i = 1;
            if (i < tokens.size() && tokens[i] == "startpos") {
                board.set_startpos();
                ++i;
            } else if (i < tokens.size() && tokens[i] == "fen") {
                std::string fen;
                ++i;
                while (i < tokens.size() && tokens[i] != "moves") {
                    if (!fen.empty()) fen.push_back(' ');
                    fen += tokens[i++];
                }
                board.set_fen(fen);
            }
            // apply moves
            i = 0;
            for (size_t k = 0; k < tokens.size(); ++k) if (tokens[k] == "moves") { i = k + 1; break; }
            for (; i < tokens.size(); ++i) {
                const std::string &mstr = tokens[i];
                Move m = board.parse_uci_move(mstr);
                if (m.is_null()) break;
                board.make_move(m);
            }
        } else if (cmd == "go") {
            SearchLimits limits;
            // parse go options
            for (size_t i = 1; i + 1 <= tokens.size(); ++i) {
                if (i < tokens.size() && tokens[i] == "wtime") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.wtime_ms = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "btime") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.btime_ms = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "winc") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.winc_ms = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "binc") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.binc_ms = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "movetime") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.movetime_ms = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "movestogo") { int tmp = 0; if (i + 1 < tokens.size() && parse_int_str(tokens[i+1], tmp)) limits.movestogo = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "depth") { int tmp = 0; if (i + 1 < tokens.size() && parse_int_str(tokens[i+1], tmp)) limits.depth = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "nodes") { long long tmp = 0; if (i + 1 < tokens.size() && parse_ll_str(tokens[i+1], tmp)) limits.nodes = tmp; ++i; }
                else if (i < tokens.size() && tokens[i] == "infinite") { limits.infinite = true; }
            }
            search.start(limits);
        } else if (cmd == "stop") {
            search.stop();
        } else if (cmd == "quit" || cmd == "exit") {
            search.stop();
            break;
        }
    }
}