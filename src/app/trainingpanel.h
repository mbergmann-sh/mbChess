// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the panel "Training" (a dock window): the task, the source of a puzzle, the
// progress, what mbChess says about the last move, and buttons for help and the next task.
#pragma once

#include <QWidget>

class QLabel;
class QPushButton;
class Trainer;

class TrainingPanel : public QWidget
{
    Q_OBJECT
public:
    explicit TrainingPanel(Trainer *trainer, QWidget *parent = nullptr);

    void refresh();
    void retranslate();
    void showHelp(const QString &text);   // a hint or the solution

signals:
    void hintRequested();
    void solutionRequested();
    void againRequested();
    void nextRequested();
    void stopRequested();

private:
    Trainer *m_trainer;
    QLabel *m_title;
    QLabel *m_task;
    QLabel *m_source;
    QLabel *m_message;
    QLabel *m_help;
    QLabel *m_progress;
    QPushButton *m_hint;
    QPushButton *m_solution;
    QPushButton *m_again;
    QPushButton *m_next;
    QPushButton *m_stop;
};
