// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - results of the engine benchmark: the standings (sorted, winner highlighted)
// and every game with result, reason, opening, moves and duration. Updated live.
#pragma once

#include "benchmark.h"

#include <QWidget>

class QLabel;
class QPushButton;
class QTreeWidget;

class BenchmarkWindow : public QWidget
{
    Q_OBJECT
public:
    explicit BenchmarkWindow(BenchmarkRunner *runner, QWidget *parent = nullptr);

    // Rebuilds both lists from the runner's games.
    void refresh();

protected:
    void changeEvent(QEvent *event) override;
    void showEvent(QShowEvent *event) override;

private:
    void saveGames();
    void retranslateUi();

    BenchmarkRunner *m_runner;
    QLabel *m_summary = nullptr;
    QLabel *m_standingsLabel = nullptr;
    QLabel *m_gamesLabel = nullptr;
    QTreeWidget *m_standings = nullptr;
    QTreeWidget *m_games = nullptr;
    QPushButton *m_saveButton = nullptr;
    QPushButton *m_closeButton = nullptr;
    int m_status = 0; // 0 running, 1 finished, 2 cancelled
};
