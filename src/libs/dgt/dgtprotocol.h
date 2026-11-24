// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - DGT e-board protocol (DGT "Chessboard Communication Protocol Description"
// 1.2.1, 2024): command and message codes, piece codes, field numbering and the
// receiver state machine. Commands are single bytes; messages start with a byte with
// the MSB set, followed by a 14-bit length (two 7-bit bytes, header included) and data.
#pragma once

#include <chesscore/types.h>

#include <QByteArray>
#include <QString>
#include <QVector>

namespace mbc::dgt {

// App -> board
enum Command : quint8 {
    SendReset = 0x40,
    SendBoard = 0x42,
    SendUpdateBoard = 0x44,
    ReturnSerialNr = 0x45,
    SendTrademark = 0x47,
    SendHardwareVersion = 0x48,
    SendBatteryStatus = 0x4C,
    SendVersion = 0x4D,
    ReturnLongSerialNr = 0x55,
};

// Board -> app
enum MessageId : quint8 {
    MsgBoardDump = 0x86,
    MsgFieldUpdate = 0x8E,
    MsgSerialNr = 0x91,
    MsgTrademark = 0x92,
    MsgVersion = 0x93,
    MsgHardwareVersion = 0x96,
    MsgBatteryStatus = 0xA0,
    MsgLongSerialNr = 0xA2,
};

// Serial settings of all DGT boards: 9600 baud, 8 data bits, no parity, 1 stop bit.
constexpr int BaudRate = 9600;
// USB vendor id of the DGT USB boards ("RSK USB Serial Port", VID 045B PID 8111).
constexpr quint16 UsbVendorId = 0x045B;
// After the last field update, wait this long before the board counts as settled.
constexpr int QuiescenceMs = 300;

struct Message
{
    int id = 0;
    QByteArray data; // without the 3 header bytes
};

// Byte stream -> messages; bytes before a message start are skipped.
class Receiver
{
public:
    // Returns the messages completed by these bytes.
    QVector<Message> feed(const QByteArray &bytes);
    void reset();

private:
    enum State { WaitMessage, LengthHi, LengthLo, Data };
    State m_state = WaitMessage;
    int m_id = 0;
    int m_length = 0;
    QByteArray m_data;
};

// DGT piece code (0 empty, 1..12) <-> mbChess piece; unknown codes give NoPiece.
Piece pieceFromCode(int code);
int codeFromPiece(Piece p);
// DGT field 0 = a8 ... 63 = h1 (White at the bottom); flipped: the board is turned.
int squareFromField(int field, bool flipped = false);
int fieldFromSquare(int square, bool flipped = false);

// 64 pieces in mbChess square order (a1 = 0) from a board dump.
QVector<Piece> boardFromDump(const QByteArray &data, bool flipped = false);
// Piece placement part of a FEN ("rnbqkbnr/pppppppp/8/...").
QString placementFen(const QVector<Piece> &board);
// The initial position in mbChess square order.
QVector<Piece> initialBoard();

} // namespace mbc::dgt
