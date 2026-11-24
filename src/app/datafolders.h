// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - data folders next to the program (as in Fritz): "Openings" for opening books
// (PolyGlot .bin), "Chessbooks" for game databases (PGN) the engine learns from and
// "Syzygy" for endgame tablebases (.rtbw/.rtbz).
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

// Creates the folders if they are missing (fails silently where the program folder
// is read-only).
inline void create()
{
    QDir().mkpath(openingsFolder());
    QDir().mkpath(chessbooksFolder());
    QDir().mkpath(syzygyFolder());
}

} // namespace datafolders
