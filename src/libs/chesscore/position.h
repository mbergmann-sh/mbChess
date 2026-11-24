// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - a chess position: board, side to move, castling, en passant, clocks
#pragma once

#include "move.h"
#include "types.h"

#include <QString>

#include <vector>

namespace mbc {

using MoveList = std::vector<Move>;

class Position
{
public:
    static const char *const StartFen;

    Position(); // empty board, White to move
    static Position startPosition();

    // Parses a FEN string. Returns false and fills *error on invalid input;
    // *this is left unchanged in that case. Four-field FENs are accepted.
    bool setFromFen(const QString &fen, QString *error = nullptr);
    QString toFen() const;

    Piece pieceAt(int sq) const { return m_board[sq]; }
    Color sideToMove() const { return m_side; }
    int castlingRights() const { return m_castling; }
    int enPassantSquare() const { return m_epSquare; }
    int halfmoveClock() const { return m_halfmove; }
    int fullmoveNumber() const { return m_fullmove; }
    std::uint64_t hash() const { return m_hash; }

    Bitboard pieces(Color c, PieceType t) const { return m_bb[makePiece(c, t)]; }
    Bitboard occupancy(Color c) const { return m_occ[c]; }
    Bitboard occupancy() const { return m_occ[White] | m_occ[Black]; }
    int kingSquare(Color c) const;

    bool isSquareAttacked(int sq, Color by) const;
    bool inCheck() const
    {
        const int k = kingSquare(m_side);
        return k != NoSquare && isSquareAttacked(k, ~m_side);
    }
    bool isCapture(Move m) const;

    MoveList legalMoves() const;
    bool isLegal(Move m) const;

    // Applies a move produced by legalMoves() (or parsed from SAN/UCI).
    void makeMove(Move m);
    Position after(Move m) const { Position p(*this); p.makeMove(m); return p; }

    bool isCheckmate() const;
    bool isStalemate() const;
    bool isInsufficientMaterial() const;
    bool isFiftyMoveDraw() const { return m_halfmove >= 100; }
    // Status that follows from this position alone (repetition needs the game history).
    GameStatus status() const;

    // Free editing (board editor): no legality checks. Castling rights without king and
    // rook on their squares are dropped, an en passant square that does not fit is cleared.
    // Check the result with isValid() before moves are generated.
    void setPiece(int sq, Piece p);
    void setSideToMove(Color c);
    void setCastlingRights(int rights);
    void setEnPassantSquare(int sq);
    void setMoveCounters(int halfmove, int fullmove);
    // Castling rights the pieces allow (king and rook on their start squares).
    int possibleCastlingRights() const;
    // The rules setFromFen() checks: one king each, no pawn on rank 1/8, side not to
    // move not in check.
    bool isValid(QString *error = nullptr) const;

    bool operator==(const Position &o) const;
    bool operator!=(const Position &o) const { return !(*this == o); }

private:
    void clear();
    void putPiece(Piece p, int sq);
    void removePiece(int sq);
    void computeHash();
    void generatePseudoLegal(MoveList &moves) const;

    Bitboard m_bb[12] {};
    Bitboard m_occ[2] {};
    Piece m_board[64];
    Color m_side = White;
    int m_castling = NoCastling;
    int m_epSquare = NoSquare;
    int m_halfmove = 0;
    int m_fullmove = 1;
    std::uint64_t m_hash = 0;
};

// Counts leaf nodes of the legal move tree (for testing the move generator).
std::uint64_t perft(const Position &pos, int depth);

QString squareName(int sq);
int parseSquare(const QString &name); // returns NoSquare on error

} // namespace mbc
