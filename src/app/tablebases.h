// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - endgame tablebases: where they are, which ones are there, and passing the
// folders to the engines that can use them.
// Syzygy (UCI option "SyzygyPath"): Stockfish, Arasan, Viridithas, Texel, Patricia;
// Crafty through its "tbpath"/"egtb" commands.
// Gaviota (UCI option "GaviotaTbPath", distance to mate): Texel. mbChess itself reads
// the Gaviota tables too and shows the result of positions with up to 5 pieces.
#pragma once

#include <QString>
#include <QStringList>

namespace mbc {
class Position;
namespace engine {
class UciEngine;
}
}

namespace tablebases {

// Settings (menu Engines > Endgame tablebases)
bool enabled();
void setEnabled(bool on);
QString folder();                 // default: the folder "Syzygy" next to the program
void setFolder(const QString &path);
// The folder handed to the engines, empty when switched off or without tables.
QString activePath();

// Gaviota tables (files *.gtb.cp4), switched on and off together with the Syzygy ones.
QString gaviotaFolder();          // default: the folder "Gaviota" next to the program
void setGaviotaFolder(const QString &path);
QString gaviotaActivePath();

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
// The same for Gaviota (wdl = .gtb.cp4 files found, dtz unused).
Status gaviotaStatus(const QString &folder);

// Opens the Gaviota tables of gaviotaActivePath() for mbChess itself (or closes them).
void openGaviotaForGui();
// "White mates in 5", "Draw" ... for a position with up to 5 pieces, empty if the
// tables do not know it.
QString gaviotaText(const mbc::Position &pos);

// True for the engine options mbChess sets itself.
bool isTablebaseOption(const QString &name);
// Sets the tablebase folders in the engine, if it has matching options. Call on
// optionsKnown, before the engine's first "isready".
void applyToEngine(mbc::engine::UciEngine *engine, const QString &path,
                   const QString &gaviotaPath = gaviotaActivePath());

// Approximate download sizes of the 3-4-5 piece tables.
constexpr qint64 WdlBytes = 378LL * 1024 * 1024;
constexpr qint64 DtzBytes = 561LL * 1024 * 1024;

} // namespace tablebases
