// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - Gaviota endgame tablebases (positions with up to 5 pieces, distance to
// mate) for mbChess positions. One set of tables at a time for the whole program;
// all functions may be called from any thread.
#pragma once

#include <chesscore/position.h>

#include <QString>

namespace mbc::gaviota {

// Loads the tables of a folder (files *.gtb.cp4). An empty folder closes them.
// Returns false if no table was found there.
bool open(const QString &folder, int cacheMegabytes = 16);
void close();
bool isOpen();
QString folder();

// Which tables open() found (tb_availability): for 3, 4 and 5 pieces.
struct Availability
{
    bool some[6] = {};      // index = number of pieces
    bool complete[6] = {};
    int completeUpTo() const;   // 0: not even all 3-piece tables
};
Availability availability();

struct Result
{
    enum Kind { Draw, WhiteMates, BlackMates } kind = Draw;
    int plies = 0;          // to mate, from the position (0 for a draw or a mate on the board)
    int movesToMate() const { return (plies + 1) / 2; }
};
// False if the position is not in the loaded tables (more than 5 pieces, castling
// rights, a missing table).
bool probe(const Position &pos, Result *result);

// Number of pieces on the board, kings included.
int pieceCount(const Position &pos);

} // namespace mbc::gaviota
