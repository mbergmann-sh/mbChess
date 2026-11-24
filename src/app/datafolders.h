// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - data folders next to the program (as in Fritz): "Openings" for opening books
// (PolyGlot .bin), "Chessbooks" for game databases (PGN) the engine learns from and
// "Syzygy" and "Gaviota" for endgame tablebases (.rtbw/.rtbz, .gtb.cp4), "Training" for
// more puzzles.
#pragma once

#include <QCoreApplication>
#include <QDir>
#include <QString>

namespace datafolders {

inline QString openingsFolder()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/Openings");
}

inline QString chessbooksFolder()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/Chessbooks");
}

inline QString syzygyFolder()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/Syzygy");
}

inline QString gaviotaFolder()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/Gaviota");
}

// More puzzles for Training > Find the mate (mate2*.txt in the mbChess format, Lichess CSV).
inline QString trainingFolder()
{
    return QCoreApplication::applicationDirPath() + QStringLiteral("/Training");
}

// Creates the folders if they are missing (fails silently where the program folder
// is read-only).
inline void create()
{
    QDir().mkpath(openingsFolder());
    QDir().mkpath(chessbooksFolder());
    QDir().mkpath(syzygyFolder());
    QDir().mkpath(gaviotaFolder());
    QDir().mkpath(trainingFolder());
}

} // namespace datafolders
