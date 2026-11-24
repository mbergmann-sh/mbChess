// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - king and pawn against king: an exact win/draw table, computed in memory on
// first use (about 0.5 MB, a fraction of a second). The training "Endgames" uses it to
// check pawn endings without endgame tablebases on disk.
#pragma once

#include "position.h"

namespace mbc::kpk {

enum class Result { Unknown, Draw, Win };   // Win: the side with the pawn wins

// The result of a position with exactly two kings and one pawn (Unknown otherwise).
Result probe(const Position &pos);
// The colour of the pawn in such a position.
Color strongSide(const Position &pos);

} // namespace mbc::kpk
