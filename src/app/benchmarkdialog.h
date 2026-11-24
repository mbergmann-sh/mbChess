// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - settings for the engine benchmark: which engines take part (check boxes,
// all checked by default), time control (5 min + 3 s), games per pairing, resources.
#pragma once

#include "benchmark.h"

#include <QDialog>

class QCheckBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;

class BenchmarkDialog : public QDialog
{
    Q_OBJECT
public:
    explicit BenchmarkDialog(QWidget *parent = nullptr);

    BenchmarkSettings settings() const;
    void accept() override;

    // Text like "3 h 20 min" for a duration.
    static QString durationText(qint64 ms);
    // Clock time in ms from now: "14:30" today, else with the date.
    static QString endTimeText(qint64 ms);

private:
    void updateEstimate();
    void setAllChecked(bool checked);

    QVector<EngineEntry> m_engines;
    QListWidget *m_list = nullptr;
    QSpinBox *m_minutes = nullptr;
    QSpinBox *m_increment = nullptr;
    QSpinBox *m_games = nullptr;
    QSpinBox *m_threads = nullptr;
    QSpinBox *m_hash = nullptr;
    QCheckBox *m_limit = nullptr;
    QSpinBox *m_elo = nullptr;
    QCheckBox *m_tablebases = nullptr;
    QCheckBox *m_follow = nullptr;
    QCheckBox *m_showResults = nullptr;
    QLabel *m_estimate = nullptr;
    QPushButton *m_startButton = nullptr;
};
