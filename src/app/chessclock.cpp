// SPDX-License-Identifier: GPL-3.0-or-later
#include "chessclock.h"

#include <QCoreApplication>
#include <QGridLayout>
#include <QLabel>
#include <QStringList>
#include <QTimer>

using namespace mbc;

QString TimeControl::pgnTag() const
{
    if (!enabled())
        return QStringLiteral("-");
    QString s = QString::number(baseMs / 1000);
    if (incrementMs > 0)
        s += QLatin1Char('+') + QString::number(incrementMs / 1000);
    return s;
}

QString TimeControl::label() const
{
    if (!enabled())
        return QCoreApplication::translate("TimeControl", "No clock");
    const qint64 sec = baseMs / 1000;
    QString base = sec % 60 == 0 ? QCoreApplication::translate("TimeControl", "%1 min").arg(sec / 60)
                                 : QCoreApplication::translate("TimeControl", "%1 s").arg(sec);
    if (incrementMs > 0)
        base += QCoreApplication::translate("TimeControl", " + %1 s").arg(incrementMs / 1000);
    return base;
}

TimeControl TimeControl::fromPgnTag(const QString &tag)
{
    TimeControl tc;
    const QStringList parts = tag.split(QLatin1Char('+'));
    bool ok = false;
    const qint64 base = parts.value(0).toLongLong(&ok);
    if (!ok)
        return tc;
    tc.baseMs = base * 1000;
    if (parts.size() > 1)
        tc.incrementMs = parts.at(1).toLongLong() * 1000;
    return tc;
}

ChessClock::ChessClock(QObject *parent)
    : QObject(parent)
    , m_timer(new QTimer(this))
{
    m_timer->setInterval(100);
    connect(m_timer, &QTimer::timeout, this, &ChessClock::tick);
}

void ChessClock::setTimeControl(const TimeControl &tc)
{
    stop();
    m_tc = tc;
    m_remaining[White] = m_remaining[Black] = tc.baseMs;
    m_active = White;
    emit changed();
}

void ChessClock::start(Color side)
{
    if (!m_tc.enabled())
        return;
    if (m_running)
        tick();
    m_active = side;
    m_running = true;
    m_elapsed.start();
    m_timer->start();
    emit changed();
}

void ChessClock::switchSide()
{
    if (!m_tc.enabled())
        return;
    if (m_running)
        tick();
    m_remaining[m_active] += m_tc.incrementMs;
    start(~m_active);
}

void ChessClock::stop()
{
    if (m_running)
        tick();
    m_running = false;
    m_timer->stop();
    emit changed();
}

void ChessClock::setRemaining(Color side, qint64 ms)
{
    m_remaining[side] = ms;
    emit changed();
}

qint64 ChessClock::remaining(Color side) const
{
    if (m_running && side == m_active)
        return qMax<qint64>(0, m_remaining[side] - m_elapsed.elapsed());
    return m_remaining[side];
}

void ChessClock::tick()
{
    if (!m_running)
        return;
    m_remaining[m_active] -= m_elapsed.restart();
    if (m_remaining[m_active] <= 0) {
        m_remaining[m_active] = 0;
        m_running = false;
        m_timer->stop();
        emit changed();
        emit flagFell(m_active);
        return;
    }
    emit changed();
}

// ---------------------------------------------------------------------------

ClockWidget::ClockWidget(ChessClock *clock, QWidget *parent)
    : QWidget(parent)
    , m_clock(clock)
{
    auto *grid = new QGridLayout(this);
    grid->setContentsMargins(0, 0, 0, 0);
    for (int c = 0; c < 2; ++c) {
        m_panel[c] = new QWidget(this);
        m_panel[c]->setObjectName(c == White ? QStringLiteral("whiteClock") : QStringLiteral("blackClock"));
        m_panel[c]->setAttribute(Qt::WA_StyledBackground, true);
        auto *v = new QGridLayout(m_panel[c]);
        m_name[c] = new QLabel(m_panel[c]);
        m_name[c]->setAlignment(Qt::AlignCenter);
        m_time[c] = new QLabel(m_panel[c]);
        m_time[c]->setAlignment(Qt::AlignCenter);
        QFont f = m_time[c]->font();
        f.setPointSizeF(f.pointSizeF() * 2.2);
        f.setBold(true);
        f.setStyleHint(QFont::Monospace);
        m_time[c]->setFont(f);
        v->addWidget(m_name[c], 0, 0);
        v->addWidget(m_time[c], 1, 0);
        grid->addWidget(m_panel[c], 0, c);
    }
    connect(clock, &ChessClock::changed, this, &ClockWidget::refresh);
    refresh();
}

void ClockWidget::setNames(const QString &white, const QString &black)
{
    m_name[White]->setText(QStringLiteral("♔ ") + white);
    m_name[Black]->setText(QStringLiteral("♚ ") + black);
}

QString ClockWidget::format(qint64 ms)
{
    if (ms < 10000) // tenths below ten seconds
        return QStringLiteral("0:%1.%2").arg(ms / 1000, 2, 10, QLatin1Char('0')).arg((ms % 1000) / 100);
    const qint64 s = (ms + 999) / 1000;
    if (s >= 3600)
        return QStringLiteral("%1:%2:%3").arg(s / 3600).arg((s / 60) % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

void ClockWidget::refresh()
{
    const bool enabled = m_clock->timeControl().enabled();
    for (int c = 0; c < 2; ++c) {
        const Color side = Color(c);
        const qint64 ms = m_clock->remaining(side);
        m_time[c]->setText(enabled ? format(ms) : QStringLiteral("–:––"));
        const bool active = m_clock->isRunning() && m_clock->activeSide() == side;
        QString style = QStringLiteral("QWidget#%1 { border-radius: 6px; background: %2; }"
                                       " QLabel { color: %3; background: transparent; }")
                            .arg(m_panel[c]->objectName(),
                                 active ? QStringLiteral("#1f4f9e") : QStringLiteral("#2b2b2b"),
                                 enabled && ms < 10000 && active ? QStringLiteral("#ff8080")
                                                                 : QStringLiteral("#f2f2f2"));
        m_panel[c]->setStyleSheet(style);
    }
}
