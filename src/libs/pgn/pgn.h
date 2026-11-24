// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - reading and writing PGN (Portable Game Notation).
// Phase 1: main line only; comments, NAGs and variations are skipped when reading.
#pragma once

#include <chesscore/game.h>

#include <QByteArray>
#include <QString>
#include <QStringList>
#include <QVector>

namespace mbc::pgn {

// Writes one game: Seven Tag Roster first, other tags sorted, SetUp/FEN for non-standard
// start positions, movetext wrapped at 80 columns.
QString write(const Game &game);

// Reads all games from PGN text. Problems (e.g. an illegal move) are reported in *errors;
// a game with an illegal move keeps the moves before it.
QVector<Game> read(const QString &text, QStringList *errors = nullptr);

// PGN files are UTF-8 or Latin-1 in practice: decode as UTF-8 if valid, else as Latin-1.
QString decode(const QByteArray &bytes);

bool readFile(const QString &fileName, QVector<Game> *games, QString *error);
bool appendToFile(const QString &fileName, const Game &game, QString *error);
bool writeFile(const QString &fileName, const Game &game, QString *error);

} // namespace mbc::pgn
