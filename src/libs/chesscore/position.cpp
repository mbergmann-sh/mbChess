// SPDX-License-Identifier: GPL-3.0-or-later
#include "position.h"

#include "attacks.h"

#include <QStringList>

namespace mbc {

const char *const Position::StartFen = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";

namespace {

// Zobrist keys. Own fixed-seed generator; Polyglot book keys live in the book module.
struct Zobrist
{
    std::uint64_t piece[12][64];
    std::uint64_t castling[16];
    std::uint64_t epFile[8];
    std::uint64_t side;

    Zobrist()
    {
        std::uint64_t state = 0x6D62436865737321ULL; // "mbChess!"
        auto next = [&state]() {                     // splitmix64
            std::uint64_t z = (state += 0x9E3779B97F4A7C15ULL);
            z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
            z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
            return z ^ (z >> 31);
        };
        for (auto &p : piece)
            for (auto &k : p)
                k = next();
        for (auto &k : castling)
            k = next();
        for (auto &k : epFile)
            k = next();
        side = next();
    }
};

const Zobrist &zobrist()
{
    static const Zobrist z;
    return z;
}

// Castling rights that survive a move touching the given square.
constexpr int castlingMask(int sq)
{
    switch (sq) {
    case 0:  return AllCastling & ~WhiteQueenSide;                  // a1
    case 7:  return AllCastling & ~WhiteKingSide;                   // h1
    case 4:  return AllCastling & ~(WhiteKingSide | WhiteQueenSide); // e1
    case 56: return AllCastling & ~BlackQueenSide;                  // a8
    case 63: return AllCastling & ~BlackKingSide;                   // h8
    case 60: return AllCastling & ~(BlackKingSide | BlackQueenSide); // e8
    default: return AllCastling;
    }
}

const char kPieceChars[] = "PNBRQKpnbrqk";

Piece pieceFromChar(QChar c)
{
    const int idx = QString::fromLatin1(kPieceChars).indexOf(c);
    return idx < 0 ? NoPiece : Piece(idx);
}

void addPromotions(MoveList &moves, int from, int to)
{
    moves.emplace_back(from, to, Queen);
    moves.emplace_back(from, to, Rook);
    moves.emplace_back(from, to, Bishop);
    moves.emplace_back(from, to, Knight);
}

} // namespace

QString squareName(int sq)
{
    if (!isValidSquare(sq))
        return QStringLiteral("-");
    return QString(QChar('a' + fileOf(sq))) + QChar('1' + rankOf(sq));
}

int parseSquare(const QString &name)
{
    if (name.size() != 2)
        return NoSquare;
    const int f = name.at(0).toLower().unicode() - 'a';
    const int r = name.at(1).unicode() - '1';
    if (f < 0 || f > 7 || r < 0 || r > 7)
        return NoSquare;
    return makeSquare(f, r);
}

Position::Position()
{
    clear();
}

Position Position::startPosition()
{
    Position p;
    p.setFromFen(QString::fromLatin1(StartFen));
    return p;
}

void Position::clear()
{
    for (auto &b : m_bb)
        b = 0;
    m_occ[White] = m_occ[Black] = 0;
    for (auto &p : m_board)
        p = NoPiece;
    m_side = White;
    m_castling = NoCastling;
    m_epSquare = NoSquare;
    m_halfmove = 0;
    m_fullmove = 1;
    computeHash();
}

void Position::putPiece(Piece p, int sq)
{
    m_board[sq] = p;
    m_bb[p] |= squareBB(sq);
    m_occ[colorOf(p)] |= squareBB(sq);
}

void Position::removePiece(int sq)
{
    const Piece p = m_board[sq];
    if (p == NoPiece)
        return;
    m_bb[p] &= ~squareBB(sq);
    m_occ[colorOf(p)] &= ~squareBB(sq);
    m_board[sq] = NoPiece;
}

void Position::computeHash()
{
    const Zobrist &z = zobrist();
    std::uint64_t h = 0;
    for (int sq = 0; sq < 64; ++sq)
        if (m_board[sq] != NoPiece)
            h ^= z.piece[m_board[sq]][sq];
    h ^= z.castling[m_castling];
    if (m_epSquare != NoSquare)
        h ^= z.epFile[fileOf(m_epSquare)];
    if (m_side == Black)
        h ^= z.side;
    m_hash = h;
}

int Position::kingSquare(Color c) const
{
    const Bitboard k = m_bb[makePiece(c, King)];
    return k ? lsb(k) : NoSquare;
}

void Position::setPiece(int sq, Piece p)
{
    if (!isValidSquare(sq))
        return;
    removePiece(sq);
    if (p != NoPiece)
        putPiece(p, sq);
    m_castling &= possibleCastlingRights();
    setEnPassantSquare(m_epSquare);
}

void Position::setSideToMove(Color c)
{
    m_side = c;
    setEnPassantSquare(m_epSquare);
}

void Position::setCastlingRights(int rights)
{
    m_castling = rights & possibleCastlingRights();
    computeHash();
}

void Position::setEnPassantSquare(int sq)
{
    // valid only behind a pawn of the side that just moved, with both squares free
    if (isValidSquare(sq)) {
        const bool white = m_side == White;
        const int pawnSq = white ? sq - 8 : sq + 8;
        const int fromSq = white ? sq + 8 : sq - 8;
        if (rankOf(sq) != (white ? 5 : 2) || m_board[pawnSq] != makePiece(~m_side, Pawn)
            || m_board[sq] != NoPiece || m_board[fromSq] != NoPiece)
            sq = NoSquare;
    } else {
        sq = NoSquare;
    }
    m_epSquare = sq;
    computeHash();
}

void Position::setMoveCounters(int halfmove, int fullmove)
{
    m_halfmove = qMax(0, halfmove);
    m_fullmove = qMax(1, fullmove);
}

int Position::possibleCastlingRights() const
{
    int r = NoCastling;
    if (m_board[4] == WhiteKing) {
        if (m_board[7] == WhiteRook)
            r |= WhiteKingSide;
        if (m_board[0] == WhiteRook)
            r |= WhiteQueenSide;
    }
    if (m_board[60] == BlackKing) {
        if (m_board[63] == BlackRook)
            r |= BlackKingSide;
        if (m_board[56] == BlackRook)
            r |= BlackQueenSide;
    }
    return r;
}

bool Position::isValid(QString *error) const
{
    Position p;
    return p.setFromFen(toFen(), error);
}

bool Position::setFromFen(const QString &fen, QString *error)
{
    auto fail = [error](const QString &msg) {
        if (error)
            *error = msg;
        return false;
    };

    const QStringList f = fen.simplified().split(QLatin1Char(' '));
    if (f.size() != 4 && f.size() != 6)
        return fail(QStringLiteral("FEN must have 4 or 6 fields"));

    Position p;

    // 1. Piece placement
    const QStringList ranks = f.at(0).split(QLatin1Char('/'));
    if (ranks.size() != 8)
        return fail(QStringLiteral("FEN must describe 8 ranks"));
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i;
        int file = 0;
        for (const QChar c : ranks.at(i)) {
            if (c.isDigit()) {
                file += c.digitValue();
            } else {
                const Piece pc = pieceFromChar(c);
                if (pc == NoPiece)
                    return fail(QStringLiteral("Invalid piece character '%1'").arg(c));
                if (file > 7)
                    return fail(QStringLiteral("Rank %1 is too long").arg(rank + 1));
                if (typeOf(pc) == Pawn && (rank == 0 || rank == 7))
                    return fail(QStringLiteral("Pawn on first or last rank"));
                p.putPiece(pc, makeSquare(file, rank));
                ++file;
            }
            if (file > 8)
                return fail(QStringLiteral("Rank %1 is too long").arg(rank + 1));
        }
        if (file != 8)
            return fail(QStringLiteral("Rank %1 does not have 8 squares").arg(rank + 1));
    }
    if (popCount(p.m_bb[WhiteKing]) != 1 || popCount(p.m_bb[BlackKing]) != 1)
        return fail(QStringLiteral("Each side needs exactly one king"));

    // 2. Side to move
    if (f.at(1) == QLatin1String("w"))
        p.m_side = White;
    else if (f.at(1) == QLatin1String("b"))
        p.m_side = Black;
    else
        return fail(QStringLiteral("Side to move must be 'w' or 'b'"));

    // 3. Castling rights (rights without king/rook on their squares are dropped)
    if (f.at(2) != QLatin1String("-")) {
        for (const QChar c : f.at(2)) {
            switch (c.unicode()) {
            case 'K': p.m_castling |= WhiteKingSide; break;
            case 'Q': p.m_castling |= WhiteQueenSide; break;
            case 'k': p.m_castling |= BlackKingSide; break;
            case 'q': p.m_castling |= BlackQueenSide; break;
            default: return fail(QStringLiteral("Invalid castling field"));
            }
        }
    }
    if (p.m_board[4] != WhiteKing)
        p.m_castling &= ~(WhiteKingSide | WhiteQueenSide);
    if (p.m_board[7] != WhiteRook)
        p.m_castling &= ~WhiteKingSide;
    if (p.m_board[0] != WhiteRook)
        p.m_castling &= ~WhiteQueenSide;
    if (p.m_board[60] != BlackKing)
        p.m_castling &= ~(BlackKingSide | BlackQueenSide);
    if (p.m_board[63] != BlackRook)
        p.m_castling &= ~BlackKingSide;
    if (p.m_board[56] != BlackRook)
        p.m_castling &= ~BlackQueenSide;

    // 4. En passant square
    if (f.at(3) != QLatin1String("-")) {
        const int ep = parseSquare(f.at(3));
        if (ep == NoSquare || rankOf(ep) != (p.m_side == White ? 5 : 2))
            return fail(QStringLiteral("Invalid en passant square"));
        p.m_epSquare = ep;
    }

    // 5./6. Clocks
    if (f.size() == 6) {
        bool ok1 = false, ok2 = false;
        p.m_halfmove = f.at(4).toInt(&ok1);
        p.m_fullmove = f.at(5).toInt(&ok2);
        if (!ok1 || !ok2 || p.m_halfmove < 0 || p.m_fullmove < 1)
            return fail(QStringLiteral("Invalid move counters"));
    }

    // The side that just moved must not be in check.
    if (p.isSquareAttacked(p.kingSquare(~p.m_side), p.m_side))
        return fail(QStringLiteral("The side not to move is in check"));

    p.computeHash();
    *this = p;
    return true;
}

QString Position::toFen() const
{
    QString s;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece p = m_board[makeSquare(file, rank)];
            if (p == NoPiece) {
                ++empty;
                continue;
            }
            if (empty) {
                s += QString::number(empty);
                empty = 0;
            }
            s += QLatin1Char(kPieceChars[p]);
        }
        if (empty)
            s += QString::number(empty);
        if (rank)
            s += QLatin1Char('/');
    }
    s += m_side == White ? QLatin1String(" w ") : QLatin1String(" b ");
    QString c;
    if (m_castling & WhiteKingSide) c += QLatin1Char('K');
    if (m_castling & WhiteQueenSide) c += QLatin1Char('Q');
    if (m_castling & BlackKingSide) c += QLatin1Char('k');
    if (m_castling & BlackQueenSide) c += QLatin1Char('q');
    s += c.isEmpty() ? QStringLiteral("-") : c;
    s += QLatin1Char(' ') + squareName(m_epSquare);
    s += QStringLiteral(" %1 %2").arg(m_halfmove).arg(m_fullmove);
    return s;
}

bool Position::isSquareAttacked(int sq, Color by) const
{
    if (sq == NoSquare)
        return false;
    const Bitboard occ = occupancy();
    if (attacks::pawn(~by, sq) & pieces(by, Pawn))
        return true;
    if (attacks::knight(sq) & pieces(by, Knight))
        return true;
    if (attacks::king(sq) & pieces(by, King))
        return true;
    if (attacks::bishop(sq, occ) & (pieces(by, Bishop) | pieces(by, Queen)))
        return true;
    if (attacks::rook(sq, occ) & (pieces(by, Rook) | pieces(by, Queen)))
        return true;
    return false;
}

bool Position::isCapture(Move m) const
{
    return m.isEnPassant() || (!m.isCastling() && m_board[m.to()] != NoPiece);
}

void Position::generatePseudoLegal(MoveList &moves) const
{
    const Color us = m_side;
    const Color them = ~us;
    const Bitboard own = m_occ[us];
    const Bitboard enemy = m_occ[them];
    const Bitboard occ = own | enemy;

    // Pawns
    const int up = us == White ? 8 : -8;
    const int startRank = us == White ? 1 : 6;
    const int promoRank = us == White ? 7 : 0;
    Bitboard pawns = pieces(us, Pawn);
    while (pawns) {
        const int from = popLsb(pawns);
        const int one = from + up;
        if (!(occ & squareBB(one))) {
            if (rankOf(one) == promoRank) {
                addPromotions(moves, from, one);
            } else {
                moves.emplace_back(from, one);
                const int two = one + up;
                if (rankOf(from) == startRank && !(occ & squareBB(two)))
                    moves.emplace_back(from, two, NoPieceType, Move::DoublePush);
            }
        }
        Bitboard caps = attacks::pawn(us, from) & enemy;
        while (caps) {
            const int to = popLsb(caps);
            if (rankOf(to) == promoRank)
                addPromotions(moves, from, to);
            else
                moves.emplace_back(from, to);
        }
        if (m_epSquare != NoSquare && (attacks::pawn(us, from) & squareBB(m_epSquare)))
            moves.emplace_back(from, m_epSquare, NoPieceType, Move::EnPassant);
    }

    // Pieces
    for (PieceType pt : { Knight, Bishop, Rook, Queen, King }) {
        Bitboard bb = pieces(us, pt);
        while (bb) {
            const int from = popLsb(bb);
            Bitboard targets = 0;
            switch (pt) {
            case Knight: targets = attacks::knight(from); break;
            case Bishop: targets = attacks::bishop(from, occ); break;
            case Rook:   targets = attacks::rook(from, occ); break;
            case Queen:  targets = attacks::queen(from, occ); break;
            default:     targets = attacks::king(from); break;
            }
            targets &= ~own;
            while (targets)
                moves.emplace_back(from, popLsb(targets));
        }
    }

    // Castling (king and rook on their original squares, see setFromFen/castlingMask)
    const int kingFrom = us == White ? 4 : 60;
    const int ks = us == White ? WhiteKingSide : BlackKingSide;
    const int qs = us == White ? WhiteQueenSide : BlackQueenSide;
    if ((m_castling & (ks | qs)) && !isSquareAttacked(kingFrom, them)) {
        if ((m_castling & ks)
            && !(occ & (squareBB(kingFrom + 1) | squareBB(kingFrom + 2)))
            && !isSquareAttacked(kingFrom + 1, them) && !isSquareAttacked(kingFrom + 2, them))
            moves.emplace_back(kingFrom, kingFrom + 2, NoPieceType, Move::CastleKingSide);
        if ((m_castling & qs)
            && !(occ & (squareBB(kingFrom - 1) | squareBB(kingFrom - 2) | squareBB(kingFrom - 3)))
            && !isSquareAttacked(kingFrom - 1, them) && !isSquareAttacked(kingFrom - 2, them))
            moves.emplace_back(kingFrom, kingFrom - 2, NoPieceType, Move::CastleQueenSide);
    }
}

MoveList Position::legalMoves() const
{
    MoveList pseudo;
    pseudo.reserve(64);
    generatePseudoLegal(pseudo);

    MoveList legal;
    legal.reserve(pseudo.size());
    for (const Move m : pseudo) {
        const Position next = after(m);
        if (!next.isSquareAttacked(next.kingSquare(m_side), next.m_side))
            legal.push_back(m);
    }
    return legal;
}

bool Position::isLegal(Move m) const
{
    if (m.isNull())
        return false;
    for (const Move l : legalMoves())
        if (l == m)
            return true;
    return false;
}

void Position::makeMove(Move m)
{
    const int from = m.from();
    const int to = m.to();
    const Piece moving = m_board[from];
    const Color us = m_side;

    ++m_halfmove;
    if (typeOf(moving) == Pawn)
        m_halfmove = 0;

    if (m.isEnPassant()) {
        removePiece(us == White ? to - 8 : to + 8);
        m_halfmove = 0;
    } else if (!m.isCastling() && m_board[to] != NoPiece) {
        removePiece(to);
        m_halfmove = 0;
    }

    removePiece(from);
    putPiece(m.isPromotion() ? makePiece(us, m.promotion()) : moving, to);

    if (m.flag() == Move::CastleKingSide) {
        const Piece rook = m_board[from + 3];
        removePiece(from + 3);
        putPiece(rook, from + 1);
    } else if (m.flag() == Move::CastleQueenSide) {
        const Piece rook = m_board[from - 4];
        removePiece(from - 4);
        putPiece(rook, from - 1);
    }

    m_castling &= castlingMask(from) & castlingMask(to);
    m_epSquare = m.flag() == Move::DoublePush ? (from + to) / 2 : NoSquare;
    if (us == Black)
        ++m_fullmove;
    m_side = ~us;
    computeHash();
}

bool Position::isCheckmate() const
{
    return inCheck() && legalMoves().empty();
}

bool Position::isStalemate() const
{
    return !inCheck() && legalMoves().empty();
}

bool Position::isInsufficientMaterial() const
{
    for (Color c : { White, Black })
        if (pieces(c, Pawn) | pieces(c, Rook) | pieces(c, Queen))
            return false;

    const Bitboard knights = pieces(White, Knight) | pieces(Black, Knight);
    const Bitboard bishops = pieces(White, Bishop) | pieces(Black, Bishop);
    if (popCount(knights | bishops) <= 1)
        return true;
    if (knights)
        return false;
    // Only bishops left: draw if all of them stand on squares of one colour.
    constexpr Bitboard darkSquares = 0xAA55AA55AA55AA55ULL;
    return !(bishops & darkSquares) || !(bishops & ~darkSquares);
}

GameStatus Position::status() const
{
    if (legalMoves().empty())
        return inCheck() ? GameStatus::Checkmate : GameStatus::Stalemate;
    if (isInsufficientMaterial())
        return GameStatus::InsufficientMaterial;
    if (isFiftyMoveDraw())
        return GameStatus::FiftyMoveRule;
    return GameStatus::Ongoing;
}

bool Position::operator==(const Position &o) const
{
    for (int sq = 0; sq < 64; ++sq)
        if (m_board[sq] != o.m_board[sq])
            return false;
    return m_side == o.m_side && m_castling == o.m_castling && m_epSquare == o.m_epSquare
        && m_halfmove == o.m_halfmove && m_fullmove == o.m_fullmove;
}

std::uint64_t perft(const Position &pos, int depth)
{
    if (depth <= 0)
        return 1;
    const MoveList moves = pos.legalMoves();
    if (depth == 1)
        return moves.size();
    std::uint64_t nodes = 0;
    for (const Move m : moves)
        nodes += perft(pos.after(m), depth - 1);
    return nodes;
}

} // namespace mbc
