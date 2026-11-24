// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - basic chess types
#pragma once

#include <cstdint>

namespace mbc {

using Bitboard = std::uint64_t;

enum Color : int { White = 0, Black = 1 };
constexpr Color operator~(Color c) { return Color(c ^ 1); }

enum PieceType : int { Pawn = 0, Knight, Bishop, Rook, Queen, King, NoPieceType };

// Piece = color * 6 + type; NoPiece marks an empty square.
enum Piece : int {
    WhitePawn = 0, WhiteKnight, WhiteBishop, WhiteRook, WhiteQueen, WhiteKing,
    BlackPawn, BlackKnight, BlackBishop, BlackRook, BlackQueen, BlackKing,
    NoPiece
};

constexpr Piece makePiece(Color c, PieceType t) { return Piece(int(c) * 6 + int(t)); }
constexpr Color colorOf(Piece p) { return Color(int(p) / 6); }
constexpr PieceType typeOf(Piece p) { return p == NoPiece ? NoPieceType : PieceType(int(p) % 6); }

// Squares: a1 = 0, b1 = 1, ..., h8 = 63
constexpr int NoSquare = -1;
constexpr int makeSquare(int file, int rank) { return rank * 8 + file; }
constexpr int fileOf(int sq) { return sq & 7; }
constexpr int rankOf(int sq) { return sq >> 3; }
constexpr bool isValidSquare(int sq) { return sq >= 0 && sq < 64; }
constexpr Bitboard squareBB(int sq) { return Bitboard(1) << sq; }

enum CastlingRight : int {
    NoCastling = 0,
    WhiteKingSide = 1, WhiteQueenSide = 2,
    BlackKingSide = 4, BlackQueenSide = 8,
    AllCastling = 15
};

inline int popCount(Bitboard b)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_popcountll(b);
#else
    int n = 0;
    while (b) { b &= b - 1; ++n; }
    return n;
#endif
}

// Index of the least significant set bit; b must not be 0.
inline int lsb(Bitboard b)
{
#if defined(__GNUC__) || defined(__clang__)
    return __builtin_ctzll(b);
#else
    int i = 0;
    while (!(b & 1)) { b >>= 1; ++i; }
    return i;
#endif
}

// Index of the most significant set bit; b must not be 0.
inline int msb(Bitboard b)
{
#if defined(__GNUC__) || defined(__clang__)
    return 63 - __builtin_clzll(b);
#else
    int i = 63;
    while (!(b & (Bitboard(1) << i))) --i;
    return i;
#endif
}

inline int popLsb(Bitboard &b)
{
    const int s = lsb(b);
    b &= b - 1;
    return s;
}

enum class GameStatus {
    Ongoing,
    Checkmate,
    Stalemate,
    InsufficientMaterial,
    FiftyMoveRule,
    ThreefoldRepetition // only reported by Game, which knows the history
};

} // namespace mbc
