// SPDX-License-Identifier: GPL-3.0-or-later
#include "dgtboard.h"

#include <QSerialPort>

namespace mbc::dgt {

Board::Board(QObject *parent)
    : QObject(parent)
    , m_fields(64, '\0')
{
    m_settleTimer.setSingleShot(true);
    m_settleTimer.setInterval(QuiescenceMs);
    connect(&m_settleTimer, &QTimer::timeout, this, &Board::settle);
    m_answerTimer.setSingleShot(true);
    m_answerTimer.setInterval(3000);
    connect(&m_answerTimer, &QTimer::timeout, this, [this] {
        if (m_connected)
            return;
        if (++m_retries <= 2) { // ask again: Bluetooth links can lose the first bytes
            send(QByteArray(1, char(SendBoard)) + char(SendUpdateBoard));
            m_answerTimer.start();
            return;
        }
        fail(tr("The DGT board on %1 does not answer.").arg(m_portName));
    });
    m_aliveTimer.setInterval(4000);
    connect(&m_aliveTimer, &QTimer::timeout, this, [this] {
        if (++m_unanswered > 2) {
            fail(tr("The DGT board on %1 does not answer any more (switched off?).").arg(m_portName));
            return;
        }
        send(QByteArray(1, char(ReturnSerialNr)));
    });
}

Board::~Board()
{
    close();
}

bool Board::open(const QString &portName, QString *error)
{
    close();
    m_port = new QSerialPort(portName, this);
    m_port->setBaudRate(BaudRate);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        if (error)
            *error = tr("The port %1 cannot be opened: %2").arg(portName, m_port->errorString());
        delete m_port;
        m_port = nullptr;
        return false;
    }
    m_port->setDataTerminalReady(true);
    m_port->clear();
    m_portName = portName;
    m_receiver.reset();
    m_fields = QByteArray(64, '\0');
    m_connected = false;
    m_retries = 0;
    m_trademark.clear();
    m_version.clear();
    connect(m_port, &QSerialPort::readyRead, this, &Board::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        if (e == QSerialPort::ResourceError || e == QSerialPort::PermissionError)
            fail(tr("The connection to the DGT board on %1 was lost.").arg(m_portName));
    });
    // recommended sequence: reset, trademark, version, board dump, update mode
    send(QByteArray(1, char(SendReset)));
    send(QByteArray(1, char(SendTrademark)) + char(SendVersion) + char(SendBoard) + char(SendUpdateBoard));
    m_answerTimer.start();
    return true;
}

void Board::close()
{
    m_settleTimer.stop();
    m_answerTimer.stop();
    m_aliveTimer.stop();
    if (m_port) {
        m_port->disconnect(this);
        if (m_port->isOpen()) {
            m_port->write(QByteArray(1, char(SendReset))); // leave update mode
            m_port->waitForBytesWritten(100);
            m_port->close();
        }
        m_port->deleteLater();
        m_port = nullptr;
    }
    m_connected = false;
}

bool Board::isOpen() const
{
    return m_port && m_port->isOpen();
}

void Board::setFlipped(bool flipped)
{
    if (flipped == m_flipped)
        return;
    m_flipped = flipped;
    emit flippedChanged(flipped);
    if (m_connected)
        emit boardChanged();
}

void Board::send(const QByteArray &bytes)
{
    if (m_port && m_port->isOpen())
        m_port->write(bytes);
}

void Board::onReadyRead()
{
    if (!m_port)
        return;
    const QVector<Message> messages = m_receiver.feed(m_port->readAll());
    for (const Message &m : messages)
        handle(m);
}

void Board::handle(const Message &m)
{
    m_unanswered = 0; // the board is alive
    switch (m.id) {
    case MsgTrademark:
        m_trademark = QString::fromLatin1(m.data).simplified();
        break;
    case MsgVersion:
        if (m.data.size() >= 2)
            m_version = QStringLiteral("%1.%2").arg(int(quint8(m.data.at(0)))).arg(int(quint8(m.data.at(1))));
        break;
    case MsgBoardDump:
        if (m.data.size() < 64)
            break;
        m_fields = m.data.left(64);
        if (!m_connected) {
            m_connected = true;
            m_answerTimer.stop();
            m_unanswered = 0;
            m_aliveTimer.start();
            emit connected();
        }
        settle(); // a dump is complete: no need to wait
        break;
    case MsgFieldUpdate:
        if (m.data.size() >= 2) {
            const int field = quint8(m.data.at(0));
            if (field < 64)
                m_fields[field] = m.data.at(1);
            m_settleTimer.start();
        }
        break;
    default:
        break;
    }
}

void Board::settle()
{
    m_settleTimer.stop();
    if (m_autoFlip) {
        const QVector<Piece> start = initialBoard();
        if (boardFromDump(m_fields, !m_flipped) == start && boardFromDump(m_fields, m_flipped) != start) {
            m_flipped = !m_flipped;
            emit flippedChanged(m_flipped);
        }
    }
    emit boardChanged();
}

void Board::fail(const QString &reason)
{
    if (!m_port)
        return;
    close();
    emit connectionLost(reason);
}

} // namespace mbc::dgt
