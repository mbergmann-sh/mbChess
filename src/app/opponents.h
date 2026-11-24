// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the opponents of the menu "Play": beginner levels, the computer at fixed
// strengths and opponents with a playing style. Each one names the engine that plays it
// (with fallbacks if that engine is not there), the strength and extra UCI options; the
// two weakest beginners are played by mbChess itself, without an engine.
#pragma once

#include "newgamedialog.h"

#include <chesscore/position.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

namespace opponents {

enum class Group { Beginner, Computer, Character };

struct Opponent
{
    const char *name = nullptr;    // QT_TRANSLATE_NOOP("opponents", ...)
    const char *style = nullptr;   // tooltip: how it plays
    int elo = 0;                   // approximate strength shown in the menu, 0: full strength
    int engineElo = 0;             // GameSettings::strengthElo (0: full strength)
    int builtin = GameSettings::NoBuiltin; // played by mbChess itself
    QStringList engines;           // default engine names, first one found plays
    QMap<QString, QString> options; // extra UCI options
};

QVector<Opponent> list(Group group);

QString displayName(const Opponent &o);   // translated name
QString menuText(const Opponent &o);      // "Novice (about 400 Elo)"
// The engine that plays it (empty: none of them is there; always empty for built-in players).
QString enginePath(const Opponent &o, QString *engineName = nullptr);
bool isAvailable(const Opponent &o);
QString toolTip(const Opponent &o);

// Settings for a quick start: colour, clock and name from the last "New game", the rest
// from the opponent. Beginners play without clock and books.
GameSettings quickSettings(Group group, const Opponent &o);

// The move of a built-in player.
mbc::Move builtinMove(const mbc::Position &pos, int builtin);

} // namespace opponents
