// SPDX-License-Identifier: GPL-3.0-or-later
#include "dgtscanner.h"

#include "dgtprotocol.h"

#include <QDeadlineTimer>
#include <QPointer>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QThread>

#include <algorithm>

namespace mbc::dgt {

QVector<PortInfo> availablePorts()
{
    QVector<PortInfo> ports;
    const QList<QSerialPortInfo> infos = QSerialPortInfo::availablePorts();
    for (const QSerialPortInfo &i : infos) {
        PortInfo p;
        p.name = i.portName();
        p.description = i.description();
        p.manufacturer = i.manufacturer();
        if (i.hasVendorIdentifier())
            p.vendorId = i.vendorIdentifier();
        if (i.hasProductIdentifier())
            p.productId = i.productIdentifier();
        const QString text = p.name + QLatin1Char(' ') + p.description + QLatin1Char(' ') + p.manufacturer;
        p.bluetooth = text.contains(QLatin1String("bluetooth"), Qt::CaseInsensitive)
            || p.name.startsWith(QLatin1String("rfcomm"));
        p.likelyDgt = p.vendorId == UsbVendorId || text.contains(QLatin1String("DGT"), Qt::CaseInsensitive);
        ports << p;
    }
    std::stable_sort(ports.begin(), ports.end(), [](const PortInfo &a, const PortInfo &b) {
        if (a.likelyDgt != b.likelyDgt)
            return a.likelyDgt;
        return a.bluetooth && !b.bluetooth; // then Bluetooth links (paired boards)
    });
    return ports;
}

ProbeResult probePort(const QString &portName, int timeoutMs)
{
    ProbeResult r;
    r.port = portName;
    QSerialPort sp(portName);
    sp.setBaudRate(BaudRate);
    sp.setDataBits(QSerialPort::Data8);
    sp.setParity(QSerialPort::NoParity);
    sp.setStopBits(QSerialPort::OneStop);
    sp.setFlowControl(QSerialPort::NoFlowControl);
    if (!sp.open(QIODevice::ReadWrite)) {
        r.error = sp.errorString();
        return r;
    }
    sp.setDataTerminalReady(true);
    sp.clear();
    const char reset = char(SendReset);
    sp.write(&reset, 1);
    sp.waitForBytesWritten(200);
    QThread::msleep(50);
    const char ask[3] = { char(SendTrademark), char(SendVersion), char(SendBoard) };
    sp.write(ask, 3);
    sp.waitForBytesWritten(200);

    Receiver receiver;
    bool dump = false;
    QDeadlineTimer deadline(timeoutMs);
    while (!deadline.hasExpired() && !(dump && (!r.trademark.isEmpty() || !r.version.isEmpty()))) {
        if (!sp.waitForReadyRead(int(qMax<qint64>(1, deadline.remainingTime()))))
            continue;
        for (const Message &m : receiver.feed(sp.readAll())) {
            switch (m.id) {
            case MsgTrademark:
                r.trademark = QString::fromLatin1(m.data).simplified();
                break;
            case MsgVersion:
                if (m.data.size() >= 2)
                    r.version = QStringLiteral("%1.%2").arg(int(quint8(m.data.at(0)))).arg(int(quint8(m.data.at(1))));
                break;
            case MsgBoardDump:
                if (m.data.size() >= 64) {
                    dump = true;
                    QVector<Piece> board = boardFromDump(m.data);
                    if (boardFromDump(m.data, true) == initialBoard())
                        board = boardFromDump(m.data, true);
                    r.placement = placementFen(board);
                }
                break;
            default:
                break;
            }
        }
    }
    r.found = dump || !r.trademark.isEmpty() || !r.version.isEmpty();
    sp.write(&reset, 1);
    sp.waitForBytesWritten(100);
    sp.close();
    return r;
}

Scanner::Scanner(QObject *parent)
    : QObject(parent)
{
    qRegisterMetaType<mbc::dgt::ProbeResult>();
}

Scanner::~Scanner()
{
    cancel();
    if (m_thread) {
        m_thread->wait(8000);
        delete m_thread;
    }
}

void Scanner::cancel()
{
    m_cancel = true;
}

void Scanner::scan(const QVector<PortInfo> &ports, bool all)
{
    if (m_thread)
        return;
    m_cancel = false;
    QPointer<Scanner> self(this);
    m_thread = QThread::create([this, self, ports, all] {
        for (const PortInfo &p : ports) {
            if (m_cancel)
                break;
            QMetaObject::invokeMethod(this, [self, name = p.name] {
                if (self)
                    emit self->probing(name);
            }, Qt::QueuedConnection);
            // Bluetooth links need longer to come up
            const ProbeResult r = probePort(p.name, p.bluetooth ? 6000 : 1500);
            QMetaObject::invokeMethod(this, [self, r] {
                if (self)
                    emit self->portProbed(r);
            }, Qt::QueuedConnection);
            if (r.found && !all)
                break;
        }
    });
    connect(m_thread, &QThread::finished, this, [this] {
        m_thread->deleteLater();
        m_thread = nullptr;
        emit finished();
    });
    m_thread->start();
}

} // namespace mbc::dgt
