// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - a single chess move
#pragma once

#include "types.h"

namespace mbc {

class Move
{
public:
    enum Flag : std::uint8_t {
        Normal = 0,
        DoublePush,
        EnPassant,
        CastleKingSide,
        CastleQueenSide
    };

    constexpr Move() = default;
    constexpr Move(int from, int to, PieceType promotion = NoPieceType, Flag flag = Normal)
        : m_from(std::uint8_t(from)), m_to(std::uint8_t(to)),
          m_promotion(std::uint8_t(promotion)), m_flag(flag), m_valid(true)
    {}

    static constexpr Move null() { return Move(); }

    constexpr bool isNull() const { return !m_valid; }
    constexpr int from() const { return m_from; }
    constexpr int to() const { return m_to; }
    constexpr PieceType promotion() const { return PieceType(m_promotion); }
    constexpr Flag flag() const { return Flag(m_flag); }
    constexpr bool isPromotion() const { return m_promotion != NoPieceType; }
    constexpr bool isCastling() const { return m_flag == CastleKingSide || m_flag == CastleQueenSide; }
    constexpr bool isEnPassant() const { return m_flag == EnPassant; }

    constexpr bool operator==(const Move &o) const
    {
        return m_valid == o.m_valid && m_from == o.m_from && m_to == o.m_to
            && m_promotion == o.m_promotion && m_flag == o.m_flag;
    }
    constexpr bool operator!=(const Move &o) const { return !(*this == o); }

private:
    std::uint8_t m_from = 0;
    std::uint8_t m_to = 0;
    std::uint8_t m_promotion = NoPieceType;
    std::uint8_t m_flag = Normal;
    bool m_valid = false;
};

} // namespace mbc
