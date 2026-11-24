// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - connection to a DGT e-board over a serial port (USB, Bluetooth Classic
// "RFCOMM", RS232). Follows the recommended sequence of the DGT protocol paper: reset,
// trademark, version, board dump, then update mode (field updates). The board counts
// as settled 300 ms after the last field update (pieces are scanned one by one, a
// move can arrive in the wrong order). A board that stops answering (switched off,
// Bluetooth out of range) is noticed by a keep-alive request every 4 s.
#pragma once

#include "dgtprotocol.h"

#include <QObject>
#include <QTimer>

class QSerialPort;

namespace mbc::dgt {

class Board : public QObject
{
    Q_OBJECT
public:
    explicit Board(QObject *parent = nullptr);
    ~Board() override;

    // Opens the port and starts the initialisation; connected() follows when the board
    // has sent its pieces. False (and *error) if the port cannot be opened.
    bool open(const QString &portName, QString *error = nullptr);
    void close();
    bool isOpen() const;
    bool isConnected() const { return m_connected; }
    QString portName() const { return m_portName; }
    QString trademark() const { return m_trademark; }
    QString version() const { return m_version; }

    // The pieces on the board in mbChess square order (a1 = 0).
    QVector<Piece> pieces() const { return boardFromDump(m_fields, m_flipped); }
    // The board is turned (Black at the connector side). With auto flip, a board that
    // shows the initial position turned is detected by itself.
    void setFlipped(bool flipped);
    bool isFlipped() const { return m_flipped; }
    void setAutoFlip(bool on) { m_autoFlip = on; }

signals:
    void connected();
    void boardChanged();               // settled pieces (also right after connected())
    void flippedChanged(bool flipped);
    void connectionLost(const QString &reason);

private:
    void onReadyRead();
    void handle(const Message &m);
    void send(const QByteArray &bytes);
    void settle();
    void fail(const QString &reason);

    QSerialPort *m_port = nullptr;
    QString m_portName;
    Receiver m_receiver;
    QByteArray m_fields;               // 64 DGT piece codes, DGT field order
    QString m_trademark;
    QString m_version;
    bool m_connected = false;
    bool m_flipped = false;
    bool m_autoFlip = true;
    int m_retries = 0;
    QTimer m_settleTimer;
    QTimer m_answerTimer;              // the board must answer the initialisation
    QTimer m_aliveTimer;               // asks for the serial number now and then
    int m_unanswered = 0;              // keep-alive requests without any answer
};

} // namespace mbc::dgt
