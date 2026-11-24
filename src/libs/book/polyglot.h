// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - opening books in the PolyGlot format (.bin), as used by many GUIs and by
// Fruit, Toga and GNU Chess. A book is a sorted list of 16-byte entries:
// key (8 bytes), move (2), weight (2), learn (4), all big-endian.
#pragma once

#include <chesscore/position.h>

#include <QByteArray>
#include <QString>
#include <QVector>

namespace mbc::book {

// The PolyGlot hash of a position.
quint64 polyglotKey(const Position &pos);

// PolyGlot move encoding (castling as "king takes rook", e.g. e1h1).
quint16 encodeMove(const Position &pos, Move m);

struct BookMove
{
    Move move;
    int weight = 0;
    quint32 learn = 0;
};

class PolyglotBook
{
public:
    // Reads the whole file (books are small). Returns false and fills *error if the
    // file cannot be read or is not a book.
    bool open(const QString &path, QString *error = nullptr);
    bool setData(const QByteArray &data, QString *error = nullptr);
    void close();
    bool isOpen() const { return !m_data.isEmpty(); }
    QString path() const { return m_path; }
    int entryCount() const { return int(m_data.size() / 16); }

    // The legal book moves of a position, best weight first.
    QVector<BookMove> moves(const Position &pos) const;
    // A move chosen by weight; `random` is any number, e.g. from QRandomGenerator.
    // Returns a null move if the position is not in the book.
    Move pick(const Position &pos, quint32 random) const;

    // One entry, for writing books (tests, tools).
    static QByteArray entry(quint64 key, quint16 move, quint16 weight, quint32 learn = 0);

private:
    QByteArray m_data;
    QString m_path;
};

} // namespace mbc::book
