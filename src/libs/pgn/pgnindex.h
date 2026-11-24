// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - index of a PGN file with many games (a game database).
// Scanning only reads the tags and counts the moves; a game is parsed when it is
// opened. Large files (hundreds of MB) are fine; scan() can report progress and be
// cancelled, so it can run in a worker thread.
#pragma once

#include <chesscore/game.h>

#include <QString>
#include <QVector>

#include <functional>

namespace mbc::pgn {

struct GameInfo
{
    qint64 offset = 0;    // byte position of the game in the file
    qint64 length = 0;    // bytes up to the next game
    QString white, black, event, site, date, round, result, eco;
    int whiteElo = 0, blackElo = 0;
    int plies = 0;        // half moves of the main line
};

class PgnIndex
{
public:
    // Progress callback: (bytes read, file size); return false to cancel.
    using Progress = std::function<bool(qint64, qint64)>;

    bool scan(const QString &fileName, QString *error = nullptr, const Progress &progress = Progress());
    // Same for PGN text in memory (tests).
    void scanText(const QByteArray &text);

    QString fileName() const { return m_fileName; }
    int size() const { return m_games.size(); }
    const GameInfo &info(int i) const { return m_games.at(i); }
    const QVector<GameInfo> &games() const { return m_games; }

    // Reads and parses game i from the file (or the scanned text).
    bool load(int i, Game *game, QString *error = nullptr) const;

private:
    void addLine(const QByteArray &line, qint64 pos);
    void finish(qint64 end);

    QString m_fileName;
    QByteArray m_text; // only for scanText()
    QVector<GameInfo> m_games;
    GameInfo m_cur;
    bool m_inGame = false;
    bool m_inMoves = false;
    int m_braceDepth = 0;   // { comment }
    int m_parenDepth = 0;   // ( variation )
    bool m_latin1 = false;  // decided per file: UTF-8 unless a tag is invalid UTF-8
};

} // namespace mbc::pgn
