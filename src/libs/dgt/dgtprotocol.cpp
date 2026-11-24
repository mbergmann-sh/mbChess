// SPDX-License-Identifier: GPL-3.0-or-later
#include "dgtprotocol.h"

namespace mbc::dgt {

QVector<Message> Receiver::feed(const QByteArray &bytes)
{
    QVector<Message> out;
    for (const char c : bytes) {
        const quint8 b = quint8(c);
        switch (m_state) {
        case WaitMessage:
            if (b & 0x80) {
                m_id = b;
                m_state = LengthHi;
            }
            break;
        case LengthHi:
            if (b & 0x80) { // a new message start: the last one was broken
                m_id = b;
                break;
            }
            m_length = b & 0x7F;
            m_state = LengthLo;
            break;
        case LengthLo:
            m_length = (m_length << 7) | (b & 0x7F);
            m_data.clear();
            if (m_length > 3) {
                m_length -= 3;
                m_state = Data;
            } else {
                out.push_back({ m_id, QByteArray() });
                m_state = WaitMessage;
            }
            break;
        case Data:
            m_data += char(b);
            if (m_data.size() >= m_length) {
                out.push_back({ m_id, m_data });
                m_data.clear();
                m_state = WaitMessage;
            }
            break;
        }
    }
    return out;
}

void Receiver::reset()
{
    m_state = WaitMessage;
    m_id = 0;
    m_length = 0;
    m_data.clear();
}

namespace {
// DGT codes 1..12: white pawn, rook, knight, bishop, king, queen, then black
const PieceType kTypes[6] = { Pawn, Rook, Knight, Bishop, King, Queen };
} // namespace

Piece pieceFromCode(int code)
{
    if (code < 1 || code > 12)
        return NoPiece;
    const Color c = code <= 6 ? White : Black;
    return makePiece(c, kTypes[(code - 1) % 6]);
}

int codeFromPiece(Piece p)
{
    if (p == NoPiece)
        return 0;
    for (int i = 0; i < 6; ++i)
        if (kTypes[i] == typeOf(p))
            return (colorOf(p) == White ? 1 : 7) + i;
    return 0;
}

int squareFromField(int field, bool flipped)
{
    if (field < 0 || field > 63)
        return NoSquare;
    if (flipped)
        field = 63 - field;
    return makeSquare(field % 8, 7 - field / 8);
}

int fieldFromSquare(int square, bool flipped)
{
    if (!isValidSquare(square))
        return -1;
    const int field = (7 - rankOf(square)) * 8 + fileOf(square);
    return flipped ? 63 - field : field;
}

QVector<Piece> boardFromDump(const QByteArray &data, bool flipped)
{
    QVector<Piece> board(64, NoPiece);
    for (int field = 0; field < 64 && field < data.size(); ++field)
        board[squareFromField(field, flipped)] = pieceFromCode(quint8(data.at(field)));
    return board;
}

QString placementFen(const QVector<Piece> &board)
{
    static const char letters[] = "PNBRQKpnbrqk";
    QString s;
    for (int rank = 7; rank >= 0; --rank) {
        int empty = 0;
        for (int file = 0; file < 8; ++file) {
            const Piece p = board.value(makeSquare(file, rank), NoPiece);
            if (p == NoPiece) {
                ++empty;
                continue;
            }
            if (empty)
                s += QString::number(empty);
            empty = 0;
            s += QLatin1Char(letters[p]);
        }
        if (empty)
            s += QString::number(empty);
        if (rank)
            s += QLatin1Char('/');
    }
    return s;
}

QVector<Piece> initialBoard()
{
    QVector<Piece> b(64, NoPiece);
    const PieceType back[8] = { Rook, Knight, Bishop, Queen, King, Bishop, Knight, Rook };
    for (int f = 0; f < 8; ++f) {
        b[makeSquare(f, 0)] = makePiece(White, back[f]);
        b[makeSquare(f, 1)] = makePiece(White, Pawn);
        b[makeSquare(f, 6)] = makePiece(Black, Pawn);
        b[makeSquare(f, 7)] = makePiece(Black, back[f]);
    }
    return b;
}

} // namespace mbc::dgt
