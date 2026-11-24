// SPDX-License-Identifier: GPL-3.0-or-later
#include "attacks.h"

namespace mbc::attacks {

namespace {

// Ray directions: the first four increase the square index, the last four decrease it.
enum Dir { North, East, NorthEast, NorthWest, South, West, SouthWest, SouthEast, DirCount };
constexpr int kFileStep[DirCount] = { 0, 1, 1, -1, 0, -1, -1, 1 };
constexpr int kRankStep[DirCount] = { 1, 0, 1, 1, -1, 0, -1, -1 };

struct Tables
{
    Bitboard pawn[2][64] {};
    Bitboard knight[64] {};
    Bitboard king[64] {};
    Bitboard ray[DirCount][64] {};

    Tables()
    {
        auto add = [](Bitboard &bb, int file, int rank) {
            if (file >= 0 && file < 8 && rank >= 0 && rank < 8)
                bb |= squareBB(makeSquare(file, rank));
        };

        for (int sq = 0; sq < 64; ++sq) {
            const int f = fileOf(sq), r = rankOf(sq);

            add(pawn[White][sq], f - 1, r + 1);
            add(pawn[White][sq], f + 1, r + 1);
            add(pawn[Black][sq], f - 1, r - 1);
            add(pawn[Black][sq], f + 1, r - 1);

            constexpr int kn[8][2] = { {1, 2}, {2, 1}, {2, -1}, {1, -2}, {-1, -2}, {-2, -1}, {-2, 1}, {-1, 2} };
            for (const auto &d : kn)
                add(knight[sq], f + d[0], r + d[1]);

            for (int df = -1; df <= 1; ++df)
                for (int dr = -1; dr <= 1; ++dr)
                    if (df || dr)
                        add(king[sq], f + df, r + dr);

            for (int d = 0; d < DirCount; ++d) {
                int ff = f + kFileStep[d], rr = r + kRankStep[d];
                while (ff >= 0 && ff < 8 && rr >= 0 && rr < 8) {
                    ray[d][sq] |= squareBB(makeSquare(ff, rr));
                    ff += kFileStep[d];
                    rr += kRankStep[d];
                }
            }
        }
    }
};

const Tables &tables()
{
    static const Tables t; // thread-safe initialisation (C++11)
    return t;
}

Bitboard slide(int sq, Bitboard occupied, Dir d)
{
    const Tables &t = tables();
    Bitboard a = t.ray[d][sq];
    const Bitboard blockers = a & occupied;
    if (blockers) {
        const int b = d < South ? lsb(blockers) : msb(blockers);
        a ^= t.ray[d][b];
    }
    return a;
}

} // namespace

Bitboard pawn(Color c, int sq) { return tables().pawn[c][sq]; }
Bitboard knight(int sq) { return tables().knight[sq]; }
Bitboard king(int sq) { return tables().king[sq]; }

Bitboard bishop(int sq, Bitboard occupied)
{
    return slide(sq, occupied, NorthEast) | slide(sq, occupied, NorthWest)
         | slide(sq, occupied, SouthEast) | slide(sq, occupied, SouthWest);
}

Bitboard rook(int sq, Bitboard occupied)
{
    return slide(sq, occupied, North) | slide(sq, occupied, South)
         | slide(sq, occupied, East) | slide(sq, occupied, West);
}

} // namespace mbc::attacks
