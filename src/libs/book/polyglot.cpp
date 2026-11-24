// SPDX-License-Identifier: GPL-3.0-or-later
#include "polyglot.h"

#include <QFile>
#include <QtEndian>

#include <algorithm>

namespace mbc::book {

namespace {

const quint64 kRandom[781] = {
#include "polyglotkeys.inc"
};
const int kCastle = 768;
const int kEnPassant = 772;
const int kTurn = 780;

// PolyGlot piece order: black pawn, white pawn, black knight, white knight, ...
int pieceKind(Piece p)
{
    return 2 * int(typeOf(p)) + (colorOf(p) == White ? 1 : 0);
}

} // namespace

quint64 polyglotKey(const Position &pos)
{
    quint64 key = 0;
    for (int sq = 0; sq < 64; ++sq) {
        const Piece p = pos.pieceAt(sq);
        if (p != NoPiece)
            key ^= kRandom[64 * pieceKind(p) + 8 * rankOf(sq) + fileOf(sq)];
    }
    const int c = pos.castlingRights();
    if (c & WhiteKingSide)
        key ^= kRandom[kCastle + 0];
    if (c & WhiteQueenSide)
        key ^= kRandom[kCastle + 1];
    if (c & BlackKingSide)
        key ^= kRandom[kCastle + 2];
    if (c & BlackQueenSide)
        key ^= kRandom[kCastle + 3];
    // En passant only counts if a pawn of the side to move can actually capture.
    const int ep = pos.enPassantSquare();
    if (ep != NoSquare) {
        const Color us = pos.sideToMove();
        const int rank = us == White ? 4 : 3; // where our capturing pawn stands
        const int file = fileOf(ep);
        const Piece pawn = makePiece(us, Pawn);
        const bool left = file > 0 && pos.pieceAt(makeSquare(file - 1, rank)) == pawn;
        const bool right = file < 7 && pos.pieceAt(makeSquare(file + 1, rank)) == pawn;
        if (left || right)
            key ^= kRandom[kEnPassant + file];
    }
    if (pos.sideToMove() == White)
        key ^= kRandom[kTurn];
    return key;
}

quint16 encodeMove(const Position &pos, Move m)
{
    int to = m.to();
    if (m.isCastling() || (typeOf(pos.pieceAt(m.from())) == King && qAbs(fileOf(m.to()) - fileOf(m.from())) == 2))
        to = makeSquare(fileOf(m.to()) > fileOf(m.from()) ? 7 : 0, rankOf(m.from()));
    int promo = 0;
    if (m.isPromotion())
        promo = int(m.promotion()); // Knight = 1 ... Queen = 4, as in PolyGlot
    return quint16(fileOf(to) | rankOf(to) << 3 | fileOf(m.from()) << 6 | rankOf(m.from()) << 9 | promo << 12);
}

bool PolyglotBook::open(const QString &path, QString *error)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    if (!setData(f.readAll(), error))
        return false;
    m_path = path;
    return true;
}

bool PolyglotBook::setData(const QByteArray &data, QString *error)
{
    if (data.isEmpty() || data.size() % 16 != 0) {
        if (error)
            *error = QStringLiteral("not a PolyGlot book");
        return false;
    }
    m_data = data;
    m_path.clear();
    return true;
}

void PolyglotBook::close()
{
    m_data.clear();
    m_path.clear();
}

QVector<BookMove> PolyglotBook::moves(const Position &pos) const
{
    QVector<BookMove> result;
    if (m_data.isEmpty())
        return result;
    const quint64 key = polyglotKey(pos);
    const uchar *base = reinterpret_cast<const uchar *>(m_data.constData());
    const auto keyAt = [base](int i) { return qFromBigEndian<quint64>(base + 16 * i); };
    // binary search for the first entry with this key
    int lo = 0, hi = entryCount();
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (keyAt(mid) < key)
            lo = mid + 1;
        else
            hi = mid;
    }
    const MoveList legal = pos.legalMoves();
    for (int i = lo; i < entryCount() && keyAt(i) == key; ++i) {
        const quint16 code = qFromBigEndian<quint16>(base + 16 * i + 8);
        const int weight = qFromBigEndian<quint16>(base + 16 * i + 10);
        const quint32 learn = qFromBigEndian<quint32>(base + 16 * i + 12);
        for (const Move &m : legal) {
            if (encodeMove(pos, m) == code) {
                result.append({ m, weight, learn });
                break;
            }
        }
    }
    std::stable_sort(result.begin(), result.end(),
                     [](const BookMove &a, const BookMove &b) { return a.weight > b.weight; });
    return result;
}

Move PolyglotBook::pick(const Position &pos, quint32 random) const
{
    const QVector<BookMove> list = moves(pos);
    qint64 total = 0;
    for (const BookMove &b : list)
        total += b.weight;
    if (list.isEmpty())
        return Move::null();
    if (total <= 0)
        return list.at(int(random % quint32(list.size()))).move;
    qint64 r = qint64(random % quint32(total));
    for (const BookMove &b : list) {
        r -= b.weight;
        if (r < 0)
            return b.move;
    }
    return list.constFirst().move;
}

QByteArray PolyglotBook::entry(quint64 key, quint16 move, quint16 weight, quint32 learn)
{
    QByteArray e(16, '\0');
    uchar *p = reinterpret_cast<uchar *>(e.data());
    qToBigEndian(key, p);
    qToBigEndian(move, p + 8);
    qToBigEndian(weight, p + 10);
    qToBigEndian(learn, p + 12);
    return e;
}

} // namespace mbc::book
