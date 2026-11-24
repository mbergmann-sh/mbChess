// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - builds an opening book from game databases ("chessbooks", PGN files), as
// Crafty does with "book create": every move of the first plies of every game is counted
// together with the game's result. Moves that were played often and scored well get a
// high weight; moves that scored badly are left out. The result is a PolyGlot book.
//
// The learn field of each entry holds the statistics: games (upper 16 bits) and the
// score of the moving side in per mille (lower 16 bits).
#pragma once

#include <chesscore/position.h>

#include <QByteArray>
#include <QHash>
#include <QPair>
#include <QVector>
#include <QString>

#include <functional>

namespace mbc::book {

struct BookStats
{
    int games = 0;
    int perMille = 0;   // score of the side that made the move, 0..1000
};
BookStats statsFromLearn(quint32 learn);

class BookBuilder
{
public:
    void setMaxPly(int plies) { m_maxPly = plies; }
    void setMinGames(int games) { m_minGames = games; }

    // Reads a PGN file game by game (also very large files). `progress` gets the bytes
    // read and the file size and may return false to cancel; then false is returned.
    bool addPgnFile(const QString &path, QString *error = nullptr,
                    const std::function<bool(qint64, qint64)> &progress = {});
    // The same for PGN text in memory (tests).
    void addPgnText(const QByteArray &text);

    int gameCount() const { return m_games; }
    int positionCount() const { return int(m_stats.size()); }

    // The book, sorted, ready to be written to a .bin file.
    QByteArray polyglotData() const;

private:
    struct MoveStat { int games = 0; int points2 = 0; }; // points2: 2 per win, 1 per draw
    void addLine(const QByteArray &line);
    void finishGame();

    int m_maxPly = 30;
    int m_minGames = 2;
    int m_games = 0;
    QHash<quint64, QHash<quint16, MoveStat>> m_stats;

    // parser state
    bool m_inComment = false;
    int m_variationDepth = 0;
    bool m_inMoves = false;
    QString m_fen;
    QByteArray m_result;
    QVector<QPair<quint64, quint16>> m_moves; // key and move of this game's first plies
    Position m_pos = Position::startPosition();
    bool m_broken = false;
};

} // namespace mbc::book
