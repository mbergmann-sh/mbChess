// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - an exact mate solver for short mates (mate in 1 to about 4 moves): a full
// width search of the attacker's moves against every defence, with a transposition
// table. Used by the training "Find the mate" to check the user's moves and to find the
// defender's most stubborn reply, and by the tool that checks the puzzle collections.
#pragma once

#include "position.h"

#include <atomic>
#include <cstdint>
#include <unordered_map>

namespace mbc {

class MateSolver
{
public:
    // cancel (optional) is polled now and then; a cancelled search answers "no mate".
    explicit MateSolver(const std::atomic<bool> *cancel = nullptr) : m_cancel(cancel) {}

    // The side to move mates in at most n moves, against any defence.
    bool canMate(const Position &pos, int n);
    // The side to move (the defender) is mated in at most n moves, whatever it plays
    // (n = 0: it is mated on the board).
    bool isMatedWithin(const Position &pos, int n) { return n <= 0 ? pos.isCheckmate() : defend(pos, n); }
    // The smallest n <= maxMoves with a forced mate, 0 if there is none.
    int mateDistance(const Position &pos, int maxMoves);
    // All first moves that mate in at most n moves (n >= 1).
    MoveList keyMoves(const Position &pos, int n);
    // The defender (to move in pos) is mated in at most n moves: the reply that delays
    // the mate longest (*movesLeft: the attacker's mate in that many moves after it).
    // Null move if the defender has no move (mate or stalemate on the board).
    Move bestDefence(const Position &pos, int n, int *movesLeft = nullptr);
    // The first move of a shortest mate in at most n (null if none).
    Move bestAttack(const Position &pos, int n, int *movesLeft = nullptr);

    std::uint64_t nodes() const { return m_nodes; }
    bool cancelled() const { return m_cancel && m_cancel->load(); }
    void clear() { m_table.clear(); }

private:
    struct Entry
    {
        std::int8_t proven = 99;    // mate in at most this many moves is proven
        std::int8_t disproven = 0;  // no mate in this many moves (or fewer)
    };
    bool attack(const Position &pos, int n);
    bool defend(const Position &pos, int n);  // the defender to move, mated in <= n?

    const std::atomic<bool> *m_cancel;
    std::unordered_map<std::uint64_t, Entry> m_table;
    std::uint64_t m_nodes = 0;
};

} // namespace mbc
