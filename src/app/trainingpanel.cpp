// SPDX-License-Identifier: GPL-3.0-or-later
#include "trainingpanel.h"

#include "trainer.h"

#include <QGridLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

TrainingPanel::TrainingPanel(Trainer *trainer, QWidget *parent) : QWidget(parent), m_trainer(trainer)
{
    auto *layout = new QVBoxLayout(this);
    m_title = new QLabel(this);
    QFont bold = m_title->font();
    bold.setBold(true);
    m_title->setFont(bold);
    m_title->setWordWrap(true);
    m_task = new QLabel(this);
    m_task->setWordWrap(true);
    m_source = new QLabel(this);
    m_source->setWordWrap(true);
    m_source->setOpenExternalLinks(true);
    m_source->setTextFormat(Qt::RichText);
    m_message = new QLabel(this);
    m_message->setWordWrap(true);
    m_message->setFont(bold);
    m_help = new QLabel(this);
    m_help->setWordWrap(true);
    m_help->setTextInteractionFlags(Qt::TextSelectableByMouse);
    m_progress = new QLabel(this);
    m_progress->setWordWrap(true);

    m_hint = new QPushButton(this);
    m_solution = new QPushButton(this);
    m_again = new QPushButton(this);
    m_next = new QPushButton(this);
    m_stop = new QPushButton(this);
    connect(m_hint, &QPushButton::clicked, this, &TrainingPanel::hintRequested);
    connect(m_solution, &QPushButton::clicked, this, &TrainingPanel::solutionRequested);
    connect(m_again, &QPushButton::clicked, this, &TrainingPanel::againRequested);
    connect(m_next, &QPushButton::clicked, this, &TrainingPanel::nextRequested);
    connect(m_stop, &QPushButton::clicked, this, &TrainingPanel::stopRequested);
    auto *buttons = new QGridLayout;
    buttons->addWidget(m_hint, 0, 0);
    buttons->addWidget(m_solution, 0, 1);
    buttons->addWidget(m_again, 1, 0);
    buttons->addWidget(m_next, 1, 1);
    buttons->addWidget(m_stop, 2, 0, 1, 2);

    layout->addWidget(m_title);
    layout->addWidget(m_task);
    layout->addWidget(m_source);
    layout->addWidget(m_message);
    layout->addWidget(m_help);
    layout->addWidget(m_progress);
    layout->addLayout(buttons);
    layout->addStretch(1);
    retranslate();
}

void TrainingPanel::retranslate()
{
    m_hint->setText(tr("&Hint"));
    m_solution->setText(tr("Show &solution"));
    m_again->setText(tr("&Try again"));
    m_stop->setText(tr("&End training"));
    refresh();
}

void TrainingPanel::showHelp(const QString &text)
{
    m_help->setText(text);
    m_help->setVisible(!text.isEmpty());
}

void TrainingPanel::refresh()
{
    const bool mate = m_trainer->kind() == Trainer::Kind::Mate || m_trainer->kind() == Trainer::Kind::Tactics;
    const bool onBoard = m_trainer->isOnBoard();
    const bool running = m_trainer->isRunning();
    m_title->setText(m_trainer->title());
    m_task->setText(m_trainer->task());
    const QString source = m_trainer->source();
    m_source->setText(source);
    m_source->setVisible(!source.isEmpty());
    QString message = onBoard ? m_trainer->message()
        : m_trainer->kind() == Trainer::Kind::None ? QString()
                                                   : tr("The task has ended: another game is on the board.");
    const char *color = nullptr;
    if (onBoard && m_trainer->state() == Trainer::State::Solved)
        color = "#2e9d3e";
    else if (onBoard && m_trainer->state() == Trainer::State::Failed)
        color = "#d0453a";
    m_message->setStyleSheet(color ? QStringLiteral("color: %1;").arg(QLatin1String(color)) : QString());
    m_message->setText(message);
    m_message->setVisible(!message.isEmpty());
    m_progress->setText(m_trainer->progress());
    if (!running)
        showHelp(QString());
    m_hint->setEnabled(running);
    m_solution->setVisible(mate);
    m_solution->setEnabled(running);
    m_again->setEnabled(m_trainer->kind() != Trainer::Kind::None);
    m_next->setEnabled(m_trainer->kind() != Trainer::Kind::None);
    m_next->setText(mate ? tr("&Next puzzle")
                         : m_trainer->kind() == Trainer::Kind::Resistance ? tr("&New game") : tr("&New position"));
}
