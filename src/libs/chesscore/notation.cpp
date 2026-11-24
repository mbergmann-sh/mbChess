// SPDX-License-Identifier: GPL-3.0-or-later
#include "notation.h"

namespace mbc {

namespace {

QChar pieceLetter(PieceType t)
{
    switch (t) {
    case Knight: return QLatin1Char('N');
    case Bishop: return QLatin1Char('B');
    case Rook:   return QLatin1Char('R');
    case Queen:  return QLatin1Char('Q');
    case King:   return QLatin1Char('K');
    default:     return QChar();
    }
}

PieceType pieceFromLetter(QChar c)
{
    switch (c.toUpper().unicode()) {
    case 'N': return Knight;
    case 'B': return Bishop;
    case 'R': return Rook;
    case 'Q': return Queen;
    case 'K': return King;
    default:  return NoPieceType;
    }
}

} // namespace

QString toSan(const Position &pos, Move m)
{
    if (m.isNull())
        return QStringLiteral("--");

    QString san;
    if (m.flag() == Move::CastleKingSide) {
        san = QStringLiteral("O-O");
    } else if (m.flag() == Move::CastleQueenSide) {
        san = QStringLiteral("O-O-O");
    } else {
        const PieceType pt = typeOf(pos.pieceAt(m.from()));
        const bool capture = pos.isCapture(m);

        if (pt == Pawn) {
            if (capture)
                san += QChar('a' + fileOf(m.from()));
        } else {
            san += pieceLetter(pt);
            // Disambiguation against other pieces of the same type reaching the same square
            bool ambiguous = false, sameFile = false, sameRank = false;
            for (const Move o : pos.legalMoves()) {
                if (o.to() != m.to() || o.from() == m.from() || typeOf(pos.pieceAt(o.from())) != pt)
                    continue;
                ambiguous = true;
                if (fileOf(o.from()) == fileOf(m.from()))
                    sameFile = true;
                if (rankOf(o.from()) == rankOf(m.from()))
                    sameRank = true;
            }
            if (ambiguous) {
                if (!sameFile)
                    san += QChar('a' + fileOf(m.from()));
                else if (!sameRank)
                    san += QChar('1' + rankOf(m.from()));
                else
                    san += squareName(m.from());
            }
        }
        if (capture)
            san += QLatin1Char('x');
        san += squareName(m.to());
        if (m.isPromotion())
            san += QLatin1Char('=') + pieceLetter(m.promotion());
    }

    const Position next = pos.after(m);
    if (next.inCheck())
        san += next.legalMoves().empty() ? QLatin1Char('#') : QLatin1Char('+');
    return san;
}

Move parseSan(const Position &pos, const QString &text)
{
    QString s = text.trimmed();
    if (s.endsWith(QLatin1String("e.p.")))
        s.chop(4);
    while (!s.isEmpty() && QStringLiteral("+#!? ").contains(s.back()))
        s.chop(1);
    if (s.isEmpty())
        return Move::null();

    const MoveList legal = pos.legalMoves();

    // Castling
    QString castle = s.toUpper();
    castle.replace(QLatin1Char('0'), QLatin1Char('O'));
    if (castle == QLatin1String("O-O") || castle == QLatin1String("O-O-O")) {
        const Move::Flag f = castle.size() == 3 ? Move::CastleKingSide : Move::CastleQueenSide;
        for (const Move m : legal)
            if (m.flag() == f)
                return m;
        return Move::null();
    }

    // Piece letter (uppercase only, so that "b4" stays a pawn move)
    PieceType pt = Pawn;
    if (s.at(0).isUpper() && pieceFromLetter(s.at(0)) != NoPieceType) {
        pt = pieceFromLetter(s.at(0));
        s.remove(0, 1);
    }

    // Promotion: "=Q", or a trailing piece letter right after the rank digit
    PieceType promo = NoPieceType;
    const int eq = s.indexOf(QLatin1Char('='));
    if (eq >= 0) {
        if (eq + 1 >= s.size())
            return Move::null();
        promo = pieceFromLetter(s.at(eq + 1));
        if (promo == NoPieceType || promo == King)
            return Move::null();
        s.truncate(eq);
    } else if (s.size() >= 3 && s.back().isLetter() && s.at(s.size() - 2).isDigit()) {
        promo = pieceFromLetter(s.back());
        if (promo == NoPieceType || promo == King)
            return Move::null();
        s.chop(1);
    }

    s.remove(QLatin1Char('x'));
    s.remove(QLatin1Char('X'));
    s.remove(QLatin1Char(':'));
    s.remove(QLatin1Char('-'));
    if (s.size() < 2)
        return Move::null();

    const int to = parseSquare(s.right(2));
    if (to == NoSquare)
        return Move::null();
    const QString hint = s.left(s.size() - 2);
    int hintFile = -1, hintRank = -1;
    for (const QChar c : hint) {
        if (c >= QLatin1Char('a') && c <= QLatin1Char('h'))
            hintFile = c.unicode() - 'a';
        else if (c >= QLatin1Char('1') && c <= QLatin1Char('8'))
            hintRank = c.unicode() - '1';
        else
            return Move::null();
    }

    Move found;
    int matches = 0;
    for (const Move m : legal) {
        if (m.to() != to || m.isCastling() || typeOf(pos.pieceAt(m.from())) != pt)
            continue;
        if (hintFile >= 0 && fileOf(m.from()) != hintFile)
            continue;
        if (hintRank >= 0 && rankOf(m.from()) != hintRank)
            continue;
        if (m.promotion() != promo)
            continue;
        found = m;
        ++matches;
    }
    return matches == 1 ? found : Move::null();
}

QString toUci(Move m)
{
    if (m.isNull())
        return QStringLiteral("0000");
    QString s = squareName(m.from()) + squareName(m.to());
    if (m.isPromotion())
        s += pieceLetter(m.promotion()).toLower();
    return s;
}

Move parseUci(const Position &pos, const QString &text)
{
    const QString s = text.trimmed().toLower();
    if (s.size() != 4 && s.size() != 5)
        return Move::null();
    const int from = parseSquare(s.mid(0, 2));
    const int to = parseSquare(s.mid(2, 2));
    const PieceType promo = s.size() == 5 ? pieceFromLetter(s.at(4)) : NoPieceType;
    if (from == NoSquare || to == NoSquare || (s.size() == 5 && promo == NoPieceType))
        return Move::null();
    for (const Move m : pos.legalMoves())
        if (m.from() == from && m.to() == to && m.promotion() == promo)
            return m;
    return Move::null();
}

QString localizeSan(const QString &san, const QString &kingQueenRookBishopKnight)
{
    if (kingQueenRookBishopKnight.size() != 5)
        return san;
    static const QString english = QStringLiteral("KQRBN");
    QString out = san;
    for (int i = 0; i < out.size(); ++i) {
        const int idx = english.indexOf(out.at(i));
        if (idx >= 0)
            out[i] = kingQueenRookBishopKnight.at(idx);
    }
    return out;
}

} // namespace mbc
