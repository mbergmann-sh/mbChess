// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - attack tables (classical ray approach, no magics needed for a GUI)
#pragma once

#include "types.h"

namespace mbc::attacks {

Bitboard pawn(Color c, int sq);
Bitboard knight(int sq);
Bitboard king(int sq);
Bitboard bishop(int sq, Bitboard occupied);
Bitboard rook(int sq, Bitboard occupied);
inline Bitboard queen(int sq, Bitboard occupied) { return bishop(sq, occupied) | rook(sq, occupied); }

} // namespace mbc::attacks
