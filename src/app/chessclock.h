// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - chess clock (base time + increment per move) and its display widget.
#pragma once

#include <chesscore/types.h>

#include <QElapsedTimer>
#include <QObject>
#include <QWidget>

class QLabel;
class QTimer;

struct TimeControl
{
    qint64 baseMs = 0;       // 0 = no clock
    qint64 incrementMs = 0;

    bool enabled() const { return baseMs > 0; }
    QString pgnTag() const;  // "300+5" or "-"
    QString label() const;   // "5 min + 3 s" (translated)
    static TimeControl fromPgnTag(const QString &tag);
};

class ChessClock : public QObject
{
    Q_OBJECT
public:
    explicit ChessClock(QObject *parent = nullptr);

    void setTimeControl(const TimeControl &tc); // resets both sides
    const TimeControl &timeControl() const { return m_tc; }

    void start(mbc::Color side);
    // Called after a move of the running side: adds the increment and starts the other side.
    void switchSide();
    void stop();
    void setRemaining(mbc::Color side, qint64 ms);

    bool isRunning() const { return m_running; }
    mbc::Color activeSide() const { return m_active; }
    qint64 remaining(mbc::Color side) const;

signals:
    void changed();
    void flagFell(mbc::Color side);

private:
    void tick();

    TimeControl m_tc;
    qint64 m_remaining[2] = { 0, 0 };
    mbc::Color m_active = mbc::White;
    bool m_running = false;
    QElapsedTimer m_elapsed;
    QTimer *m_timer = nullptr;
};

class ClockWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ClockWidget(ChessClock *clock, QWidget *parent = nullptr);
    void setNames(const QString &white, const QString &black);
    void refresh();

private:
    static QString format(qint64 ms);

    ChessClock *m_clock;
    QLabel *m_name[2];
    QLabel *m_time[2];
    QWidget *m_panel[2];
};
