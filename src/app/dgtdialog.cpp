// SPDX-License-Identifier: GPL-3.0-or-later
#include "dgtdialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSettings>
#include <QVBoxLayout>

using namespace mbc::dgt;

namespace dgtsettings {

bool enabled()
{
    return QSettings().value(QStringLiteral("dgt/enabled"), false).toBool();
}

void setEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("dgt/enabled"), on);
}

QString port()
{
    return QSettings().value(QStringLiteral("dgt/port")).toString();
}

void setPort(const QString &port)
{
    QSettings().setValue(QStringLiteral("dgt/port"), port);
}

bool autoScan()
{
    return QSettings().value(QStringLiteral("dgt/autoScan"), true).toBool();
}

bool autoDetect()
{
    return QSettings().value(QStringLiteral("dgt/autoDetect"), true).toBool();
}

int orientation()
{
    return qBound(0, QSettings().value(QStringLiteral("dgt/orientation"), 0).toInt(), 2);
}

} // namespace dgtsettings

DgtDialog::DgtDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("DGT board"));

    m_use = new QCheckBox(tr("&Use the DGT board"), this);
    m_use->setChecked(dgtsettings::enabled());

    m_ports = new QComboBox(this);
    m_ports->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    m_ports->setMinimumContentsLength(28);
    m_rescan = new QPushButton(tr("&Re-scan"), this);
    m_rescan->setToolTip(tr("Look for the ports again and check which one a DGT board answers on."));
    connect(m_rescan, &QPushButton::clicked, this, &DgtDialog::rescan);
    auto *portRow = new QHBoxLayout;
    portRow->addWidget(m_ports, 1);
    portRow->addWidget(m_rescan);

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    m_status->setMinimumHeight(60);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);

    m_autoScan = new QCheckBox(tr("If the board is not on this port, &look for it on the other ports"), this);
    m_autoScan->setChecked(dgtsettings::autoScan());

    m_autoDetect = new QCheckBox(tr("Find and use the board by itself: at &program start and whenever it is "
                                    "plugged in or switched on"), this);
    m_autoDetect->setChecked(dgtsettings::autoDetect());

    m_orientation = new QComboBox(this);
    m_orientation->addItem(tr("Detect from the initial position"));
    m_orientation->addItem(tr("White at the connector side"));
    m_orientation->addItem(tr("Black at the connector side (board turned)"));
    m_orientation->setCurrentIndex(dgtsettings::orientation());
    m_orientation->setToolTip(tr("Set up the initial position on the board and mbChess sees by itself "
                                 "which way round the board lies."));

    auto *form = new QFormLayout;
    auto *portLabel = new QLabel(tr("&Port:"), this);
    portLabel->setBuddy(m_ports);
    form->addRow(portLabel, portRow);
    form->addRow(QString(), m_status);
    form->addRow(QString(), m_autoScan);
    form->addRow(QString(), m_autoDetect);
    auto *orientationLabel = new QLabel(tr("&Orientation:"), this);
    orientationLabel->setBuddy(m_orientation);
    form->addRow(orientationLabel, m_orientation);

    auto *hint = new QLabel(tr("USB boards appear as a serial port (Windows: \"COMx\", e.g. \"RSK USB Serial "
                               "Port\"; Linux: ttyACM0). Bluetooth boards (Bluetooth Classic) have to be "
                               "paired in the system first, then they appear as a serial port too. The "
                               "DGT Pegasus (Bluetooth LE only) is not supported."), this);
    hint->setWordWrap(true);
    QFont small = hint->font();
    small.setPointSizeF(small.pointSizeF() * 0.9);
    hint->setFont(small);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    connect(buttons, &QDialogButtonBox::accepted, this, &DgtDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &DgtDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_use);
    layout->addLayout(form);
    layout->addWidget(hint);
    layout->addStretch(1);
    layout->addWidget(buttons);

    m_scanner = new Scanner(this);
    connect(m_scanner, &Scanner::probing, this, [this](const QString &port) {
        m_status->setText(tr("Checking %1 ...").arg(port));
    });
    connect(m_scanner, &Scanner::portProbed, this, &DgtDialog::onProbed);
    connect(m_scanner, &Scanner::finished, this, &DgtDialog::onScanFinished);

    resize(560, 340);
    rescan(); // look at once
}

bool DgtDialog::useBoard() const
{
    return m_use->isChecked();
}

void DgtDialog::fillPorts()
{
    const QString keep = m_ports->count() ? m_ports->currentData().toString() : dgtsettings::port();
    m_ports->clear();
    m_ports->addItem(tr("Automatic (the first board found)"), QString());
    for (const PortInfo &p : m_portInfos) {
        QString text = p.name;
        if (!p.description.isEmpty())
            text += QStringLiteral(" - ") + p.description;
        if (p.vendorId == mbc::dgt::UsbVendorId)
            text += QStringLiteral(" (DGT USB)");
        else if (p.bluetooth)
            text += QStringLiteral(" (Bluetooth)");
        m_ports->addItem(text, p.name);
    }
    // a stored port that is not there now stays selectable
    if (!keep.isEmpty() && m_ports->findData(keep) < 0)
        m_ports->addItem(tr("%1 (not available now)").arg(keep), keep);
    m_ports->setCurrentIndex(qMax(0, m_ports->findData(keep)));
}

void DgtDialog::rescan()
{
    if (m_scanner->isRunning())
        return;
    m_foundPort.clear();
    m_portInfos = availablePorts();
    fillPorts();
    // a chosen port the system does not list (e.g. some Bluetooth links) is checked first
    QVector<PortInfo> toProbe = m_portInfos;
    const QString chosen = m_ports->currentData().toString();
    bool listed = chosen.isEmpty();
    for (const PortInfo &p : m_portInfos)
        listed = listed || p.name == chosen;
    if (!listed) {
        PortInfo p;
        p.name = chosen;
        p.likelyDgt = true;
        toProbe.prepend(p);
    }
    if (toProbe.isEmpty()) {
        m_status->setText(tr("<b>No serial port found.</b> Is the board connected (and switched on)?"));
        return;
    }
    m_rescan->setEnabled(false);
    m_status->setText(tr("Looking for a DGT board ..."));
    m_scanner->scan(toProbe);
}

void DgtDialog::onProbed(const ProbeResult &r)
{
    if (!r.found)
        return;
    m_foundPort = r.port;
    const int index = m_ports->findData(r.port);
    if (index >= 0) {
        if (m_ports->itemText(index) == tr("%1 (not available now)").arg(r.port))
            m_ports->setItemText(index, r.port);
        if (!m_ports->itemText(index).endsWith(QStringLiteral(" ✓")))
            m_ports->setItemText(index, m_ports->itemText(index) + QStringLiteral(" ✓"));
        m_ports->setCurrentIndex(index);
    }
    QString name = r.trademark;
    if (name.size() > 70)
        name = name.left(67) + QStringLiteral("...");
    QString text = tr("<b>DGT board found on %1.</b>").arg(r.port.toHtmlEscaped());
    if (!name.isEmpty())
        text += QStringLiteral("<br>") + name.toHtmlEscaped();
    if (!r.version.isEmpty())
        text += QStringLiteral("<br>") + tr("Version %1").arg(r.version);
    if (!r.placement.isEmpty())
        text += QStringLiteral("<br>")
            + (r.placement == QLatin1String("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR")
                   ? tr("The board shows the initial position.")
                   : tr("Pieces on the board: %1").arg(r.placement));
    m_status->setText(text);
}

void DgtDialog::onScanFinished()
{
    m_rescan->setEnabled(true);
    if (m_foundPort.isEmpty())
        m_status->setText(tr("<b>No DGT board found.</b> Is it connected and switched on? A Bluetooth "
                             "board has to be paired first. A port in use by another program "
                             "(e.g. DGT LiveChess) cannot be checked."));
}

void DgtDialog::accept()
{
    m_scanner->cancel();
    QSettings s;
    s.setValue(QStringLiteral("dgt/enabled"), m_use->isChecked());
    s.setValue(QStringLiteral("dgt/port"), m_ports->currentData().toString());
    s.setValue(QStringLiteral("dgt/autoScan"), m_autoScan->isChecked());
    s.setValue(QStringLiteral("dgt/autoDetect"), m_autoDetect->isChecked());
    s.setValue(QStringLiteral("dgt/orientation"), m_orientation->currentIndex());
    QDialog::accept();
}
