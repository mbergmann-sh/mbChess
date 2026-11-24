// SPDX-License-Identifier: GPL-3.0-or-later
// The table is built by retrograde iteration over all positions with White's king,
// Black's king and a white pawn (side to move: 2 x 64 x 64 x 64 entries). Black pawns are
// handled by mirroring the board. A pawn on the 7th rank that promotes safely counts as a
// win; other promotions are not followed, as in the usual KPK bitbases.
#include "kpk.h"

#include "attacks.h"

#include <cstdint>
#include <cstdlib>
#include <mutex>
#include <vector>

namespace mbc::kpk {

namespace {

enum : std::uint8_t { Unknown = 0, Invalid = 1, Draw = 2, Win = 4 };

inline int index(int stm, int wk, int bk, int p)
{
    return ((stm * 64 + wk) * 64 + bk) * 64 + p;
}

inline int fileOf(int sq) { return sq & 7; }
inline int rankOf(int sq) { return sq >> 3; }
inline int distance(int a, int b)
{
    return std::max(std::abs(fileOf(a) - fileOf(b)), std::abs(rankOf(a) - rankOf(b)));
}

std::vector<std::uint8_t> g_table;
std::once_flag g_once;

std::uint8_t initial(int stm, int wk, int bk, int p)
{
    if (rankOf(p) == 0 || rankOf(p) == 7 || wk == bk || wk == p || bk == p || distance(wk, bk) <= 1)
        return Invalid;
    // White to move while Black's king is attacked by the pawn: impossible
    if (stm == White && (attacks::pawn(White, p) & squareBB(bk)))
        return Invalid;
    if (stm == White) {
        const int q = p + 8;
        if (rankOf(p) == 6 && wk != q && bk != q && (distance(bk, q) > 1 || distance(wk, q) == 1))
            return Win;
        return Unknown;
    }
    // Black to move
    const Bitboard wkAtt = attacks::king(wk);
    const Bitboard pAtt = attacks::pawn(White, p);
    const Bitboard moves = attacks::king(bk) & ~wkAtt & ~pAtt;
    if (!moves)
        return Draw; // stalemate (Black is never in check here: only the pawn could give it)
    if ((attacks::king(bk) & squareBB(p)) && !(wkAtt & squareBB(p)))
        return Draw; // takes the pawn
    return Unknown;
}

std::uint8_t classify(const std::vector<std::uint8_t> &t, int stm, int wk, int bk, int p)
{
    // White: one move to a won position wins; Black: one move to a draw draws.
    const std::uint8_t good = stm == White ? Win : Draw;
    const std::uint8_t bad = stm == White ? Draw : Win;
    bool any = false;
    bool open = false;
    bool found = false;
    auto add = [&](std::uint8_t v) {
        if (v == Invalid)
            return;
        any = true;
        if (v == good)
            found = true;
        else if (v == Unknown)
            open = true;
    };
    if (stm == White) {
        Bitboard b = attacks::king(wk);
        while (b) {
            const int to = popLsb(b);
            if (to == p || distance(to, bk) <= 1)
                continue;
            add(t[index(Black, to, bk, p)]);
        }
        if (rankOf(p) < 6) {
            const int q = p + 8;
            if (q != wk && q != bk) {
                add(t[index(Black, wk, bk, q)]);
                if (rankOf(p) == 1 && q + 8 != wk && q + 8 != bk)
                    add(t[index(Black, wk, bk, q + 8)]);
            }
        }
    } else {
        Bitboard b = attacks::king(bk) & ~attacks::king(wk) & ~attacks::pawn(White, p);
        while (b) {
            const int to = popLsb(b);
            if (to == p)
                continue; // defended (undefended captures are draws already)
            add(t[index(White, wk, to, p)]);
        }
    }
    if (found)
        return good;
    if (!any)
        return Draw; // White has no move: stalemate
    if (open)
        return Unknown;
    return bad;
}

void build()
{
    g_table.assign(2 * 64 * 64 * 64, Unknown);
    for (int stm = 0; stm < 2; ++stm)
        for (int wk = 0; wk < 64; ++wk)
            for (int bk = 0; bk < 64; ++bk)
                for (int p = 0; p < 64; ++p)
                    g_table[index(stm, wk, bk, p)] = initial(stm, wk, bk, p);
    bool changed = true;
    while (changed) {
        changed = false;
        for (int stm = 0; stm < 2; ++stm)
            for (int wk = 0; wk < 64; ++wk)
                for (int bk = 0; bk < 64; ++bk)
                    for (int p = 8; p < 56; ++p) {
                        std::uint8_t &e = g_table[index(stm, wk, bk, p)];
                        if (e != Unknown)
                            continue;
                        const std::uint8_t v = classify(g_table, stm, wk, bk, p);
                        if (v != Unknown) {
                            e = v;
                            changed = true;
                        }
                    }
    }
    // What is still unknown cannot be won.
    for (auto &e : g_table)
        if (e == Unknown)
            e = Draw;
}

} // namespace

Color strongSide(const Position &pos)
{
    return pos.pieces(White, Pawn) ? White : Black;
}

Result probe(const Position &pos)
{
    if (popCount(pos.occupancy()) != 3)
        return Result::Unknown;
    const Color strong = strongSide(pos);
    const Bitboard pawns = pos.pieces(strong, Pawn);
    if (popCount(pawns) != 1)
        return Result::Unknown;
    std::call_once(g_once, build);
    int wk = pos.kingSquare(strong);
    int bk = pos.kingSquare(~strong);
    int p = lsb(pawns);
    int stm = pos.sideToMove() == strong ? White : Black;
    if (strong == Black) { // mirror the board: the pawn becomes white
        wk ^= 56;
        bk ^= 56;
        p ^= 56;
    }
    const std::uint8_t v = g_table[index(stm, wk, bk, p)];
    if (v == Win)
        return Result::Win;
    if (v == Draw)
        return Result::Draw;
    return Result::Unknown;
}

} // namespace mbc::kpk
