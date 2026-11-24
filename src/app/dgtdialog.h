// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - DGT board settings: use the board, port (found automatically or chosen by
// hand, with "Re-scan"), orientation. On opening, the ports are probed at once.
#pragma once

#include <dgt/dgtprotocol.h>
#include <dgt/dgtscanner.h>

#include <QDialog>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;

namespace dgtsettings {
bool enabled();                 // dgt/enabled
void setEnabled(bool on);
QString port();                 // dgt/port, empty: find automatically
void setPort(const QString &port);
bool autoScan();                // dgt/autoScan: look for the board if the port fails
// dgt/autoDetect: at startup and when a board is plugged in or switched on, find and use it
bool autoDetect();
// 0 = detect (initial position turned), 1 = White at the connector side, 2 = Black
int orientation();              // dgt/orientation
}

class DgtDialog : public QDialog
{
    Q_OBJECT
public:
    explicit DgtDialog(QWidget *parent = nullptr);

    bool useBoard() const;

protected:
    void accept() override;

private:
    void rescan();
    void onProbed(const mbc::dgt::ProbeResult &r);
    void onScanFinished();
    void fillPorts();

    QCheckBox *m_use;
    QComboBox *m_ports;
    QPushButton *m_rescan;
    QLabel *m_status;
    QCheckBox *m_autoScan;
    QCheckBox *m_autoDetect;
    QComboBox *m_orientation;
    mbc::dgt::Scanner *m_scanner;
    QVector<mbc::dgt::PortInfo> m_portInfos;
    QString m_foundPort;
};
