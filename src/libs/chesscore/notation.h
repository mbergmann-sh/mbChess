// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - move notation: SAN (PGN standard) and UCI coordinate notation
#pragma once

#include "position.h"

#include <QString>

namespace mbc {

// Standard Algebraic Notation with English piece letters, e.g. "Nf3", "exd5", "e8=Q+", "O-O".
QString toSan(const Position &pos, Move m);

// Parses SAN leniently: accepts "0-0", "e8Q", "e8=q", trailing "+#!?", "e.p." and long
// algebraic input such as "Ng1f3" or "e2-e4". Returns Move::null() if the text does not
// describe exactly one legal move.
Move parseSan(const Position &pos, const QString &text);

// UCI notation, e.g. "e2e4", "e7e8q", castling as king move "e1g1".
QString toUci(Move m);
Move parseUci(const Position &pos, const QString &text);

// Replaces the English piece letters K, Q, R, B, N in a SAN string by the given letters
// (same order), e.g. localizeSan("Nf3", "KDTLS") == "Sf3" for German.
QString localizeSan(const QString &san, const QString &kingQueenRookBishopKnight);

} // namespace mbc
