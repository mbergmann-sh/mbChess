// SPDX-License-Identifier: GPL-3.0-or-later
#include "matesolver.h"

#include <algorithm>

namespace mbc {

namespace {
struct Child
{
    Move move;
    Position pos;
    int order;
};

// The attacker's moves: checks first, then captures and promotions, then the rest.
// At the last move only checks can mate.
std::vector<Child> attackerMoves(const Position &pos, bool checksOnly)
{
    std::vector<Child> list;
    const MoveList moves = pos.legalMoves();
    list.reserve(moves.size());
    for (const Move m : moves) {
        Position next = pos.after(m);
        const bool check = next.inCheck();
        if (checksOnly && !check)
            continue;
        const int order = check ? 0 : (pos.isCapture(m) || m.isPromotion()) ? 1 : 2;
        list.push_back({ m, std::move(next), order });
    }
    std::stable_sort(list.begin(), list.end(), [](const Child &a, const Child &b) { return a.order < b.order; });
    return list;
}
} // namespace

bool MateSolver::attack(const Position &pos, int n)
{
    if (n <= 0 || cancelled())
        return false;
    ++m_nodes;
    Entry &e0 = m_table[pos.hash()];
    if (e0.proven <= n)
        return true;
    if (e0.disproven >= n)
        return false;
    bool found = false;
    for (const Child &c : attackerMoves(pos, n == 1)) {
        if (c.pos.isCheckmate() || (n > 1 && defend(c.pos, n - 1))) {
            found = true;
            break;
        }
        if (cancelled())
            return false;
    }
    if (cancelled())
        return false;
    Entry &e = m_table[pos.hash()]; // the table may have grown (rehash)
    if (found)
        e.proven = std::int8_t(std::min<int>(e.proven, n));
    else
        e.disproven = std::int8_t(std::max<int>(e.disproven, n));
    return found;
}

bool MateSolver::defend(const Position &pos, int n)
{
    ++m_nodes;
    const MoveList replies = pos.legalMoves();
    if (replies.empty())
        return pos.inCheck(); // mate: yes; stalemate: no
    for (const Move r : replies) {
        if (!attack(pos.after(r), n))
            return false;
    }
    return true;
}

bool MateSolver::canMate(const Position &pos, int n)
{
    return attack(pos, n);
}

int MateSolver::mateDistance(const Position &pos, int maxMoves)
{
    for (int n = 1; n <= maxMoves; ++n) {
        if (attack(pos, n))
            return n;
        if (cancelled())
            return 0;
    }
    return 0;
}

MoveList MateSolver::keyMoves(const Position &pos, int n)
{
    MoveList keys;
    for (const Child &c : attackerMoves(pos, n == 1)) {
        if (c.pos.isCheckmate() || (n > 1 && defend(c.pos, n - 1)))
            keys.push_back(c.move);
        if (cancelled())
            return {};
    }
    return keys;
}

Move MateSolver::bestAttack(const Position &pos, int n, int *movesLeft)
{
    const int d = mateDistance(pos, n);
    if (movesLeft)
        *movesLeft = d;
    if (d == 0)
        return Move::null();
    for (const Child &c : attackerMoves(pos, d == 1)) {
        if (c.pos.isCheckmate() || (d > 1 && defend(c.pos, d - 1)))
            return c.move;
    }
    return Move::null();
}

Move MateSolver::bestDefence(const Position &pos, int n, int *movesLeft)
{
    Move best;
    int bestLeft = -1;
    for (const Move r : pos.legalMoves()) {
        const Position next = pos.after(r);
        int left = mateDistance(next, n);
        if (left == 0)
            left = 100; // no mate any more (a mistake in the puzzle): the best defence
        // equal length: prefer captures, they look like real resistance
        if (left > bestLeft || (left == bestLeft && pos.isCapture(r) && !pos.isCapture(best))) {
            best = r;
            bestLeft = left;
        }
        if (cancelled())
            break;
    }
    if (movesLeft)
        *movesLeft = bestLeft < 0 ? 0 : bestLeft;
    return best;
}

} // namespace mbc
