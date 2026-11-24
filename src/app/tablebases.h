// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - Syzygy endgame tablebases: where they are, which ones are there, and
// passing the folder to the engines that can use them (UCI option "SyzygyPath":
// Stockfish, Arasan, Viridithas; Crafty through its "tbpath"/"egtb" commands).
#pragma once

#include <QString>
#include <QStringList>

namespace mbc::engine {
class UciEngine;
}

namespace tablebases {

// Settings (menu Engines > Endgame tablebases)
bool enabled();
void setEnabled(bool on);
QString folder();                 // default: the folder "Syzygy" next to the program
void setFolder(const QString &path);
// The folder handed to the engines, empty when switched off or without tables.
QString activePath();

// The names of all Syzygy tables with up to maxPieces pieces (3..5: 145 tables),
// e.g. "KQvK", "KRPvKR" - without the extension .rtbw (WDL) / .rtbz (DTZ).
QStringList tableNames(int maxPieces = 5);

struct Status
{
    int wdl = 0;        // .rtbw files found
    int dtz = 0;        // .rtbz files found
    int expected = 0;   // tables with 3..5 pieces
    int completeUpTo = 0; // all WDL tables up to this number of pieces are there (0: none)
};
Status status(const QString &folder);

// True for the engine options mbChess sets itself.
bool isTablebaseOption(const QString &name);
// Sets the tablebase folder in the engine, if it has a matching option. Call on
// optionsKnown, before the engine's first "isready".
void applyToEngine(mbc::engine::UciEngine *engine, const QString &path);

// Approximate download sizes of the 3-4-5 piece tables.
constexpr qint64 WdlBytes = 378LL * 1024 * 1024;
constexpr qint64 DtzBytes = 561LL * 1024 * 1024;

} // namespace tablebases
