// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - finding DGT boards: lists the serial ports (USB boards have the vendor id
// 045B, Bluetooth Classic boards appear as serial ports once paired) and probes them
// in a worker thread: a port counts as a DGT board if it answers the trademark,
// version or board dump command.
#pragma once

#include <QObject>
#include <QVector>

#include <atomic>

class QThread;

namespace mbc::dgt {

struct PortInfo
{
    QString name;          // COM4, ttyACM0, cu.DGT_BT_...
    QString description;
    QString manufacturer;
    quint16 vendorId = 0;  // 0: unknown
    quint16 productId = 0;
    bool likelyDgt = false; // DGT vendor id or "DGT" in the name/description
    bool bluetooth = false;
};

// All serial ports, likely DGT boards first.
QVector<PortInfo> availablePorts();

struct ProbeResult
{
    QString port;
    bool found = false;
    QString trademark;
    QString version;
    QString placement;     // FEN piece placement of the board, if it sent its pieces
    QString error;         // port could not be opened
};

// Blocking probe of one port (call it outside the GUI thread).
ProbeResult probePort(const QString &portName, int timeoutMs);

class Scanner : public QObject
{
    Q_OBJECT
public:
    explicit Scanner(QObject *parent = nullptr);
    ~Scanner() override;

    // Probes the ports one after another (likely ones first); stops at the first board
    // found unless all is set.
    void scan(const QVector<PortInfo> &ports, bool all = false);
    void cancel();
    bool isRunning() const { return m_thread != nullptr; }

signals:
    void probing(const QString &port);
    void portProbed(const mbc::dgt::ProbeResult &result);
    void finished();

private:
    QThread *m_thread = nullptr;
    std::atomic<bool> m_cancel { false };
};

} // namespace mbc::dgt

Q_DECLARE_METATYPE(mbc::dgt::ProbeResult)
