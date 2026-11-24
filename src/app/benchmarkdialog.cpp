// SPDX-License-Identifier: GPL-3.0-or-later
#include "benchmarkdialog.h"

#include "tablebases.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDir>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QDateTime>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

BenchmarkDialog::BenchmarkDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Engine benchmark"));
    QSettings s;
    s.beginGroup(QStringLiteral("benchmark"));

    auto *layout = new QVBoxLayout(this);
    auto *intro = new QLabel(tr("The selected engines play a tournament against each other: every engine "
                                "against every other one, never against itself, with the same openings "
                                "for both colours and the same conditions for all."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    auto *engineBox = new QGroupBox(tr("Participants"), this);
    auto *engineLayout = new QVBoxLayout(engineBox);
    m_list = new QListWidget(engineBox);
    m_engines = enginemanager::engines();
    for (const EngineEntry &e : m_engines) {
        auto *item = new QListWidgetItem(e.name, m_list);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(Qt::Checked); // all take part by default
        item->setToolTip(QDir::toNativeSeparators(e.path));
    }
    engineLayout->addWidget(m_list);
    auto *allNone = new QHBoxLayout;
    auto *all = new QPushButton(tr("&All"), engineBox);
    auto *none = new QPushButton(tr("&None"), engineBox);
    connect(all, &QPushButton::clicked, this, [this] { setAllChecked(true); });
    connect(none, &QPushButton::clicked, this, [this] { setAllChecked(false); });
    allNone->addWidget(all);
    allNone->addWidget(none);
    allNone->addStretch();
    engineLayout->addLayout(allNone);
    layout->addWidget(engineBox, 1);

    auto *condBox = new QGroupBox(tr("Conditions"), this);
    auto *form = new QFormLayout(condBox);
    auto *timeRow = new QHBoxLayout;
    m_minutes = new QSpinBox(condBox);
    m_minutes->setRange(1, 180);
    m_minutes->setSuffix(tr(" min"));
    m_minutes->setValue(s.value(QStringLiteral("minutes"), 5).toInt());
    m_increment = new QSpinBox(condBox);
    m_increment->setRange(0, 120);
    m_increment->setPrefix(QStringLiteral("+ "));
    m_increment->setSuffix(tr(" s per move"));
    m_increment->setValue(s.value(QStringLiteral("increment"), 3).toInt());
    timeRow->addWidget(m_minutes);
    timeRow->addWidget(m_increment);
    timeRow->addStretch();
    form->addRow(tr("Time per game:"), timeRow);

    m_games = new QSpinBox(condBox);
    m_games->setRange(1, 20);
    m_games->setValue(s.value(QStringLiteral("games"), 2).toInt());
    m_games->setToolTip(tr("Games of each pair of engines. With an even number every opening is "
                           "played with both colours."));
    form->addRow(tr("Games per pairing:"), m_games);

    m_threads = new QSpinBox(condBox);
    m_threads->setRange(1, qMax(1, QThread::idealThreadCount()));
    m_threads->setValue(qMin(m_threads->maximum(), s.value(QStringLiteral("threads"), 1).toInt()));
    m_threads->setToolTip(tr("Threads (cores) of every engine. One thread is the fairest comparison."));
    form->addRow(tr("Threads per engine:"), m_threads);

    m_hash = new QSpinBox(condBox);
    m_hash->setRange(16, 4096);
    m_hash->setSingleStep(16);
    m_hash->setSuffix(tr(" MB"));
    m_hash->setValue(s.value(QStringLiteral("hash"), 64).toInt());
    form->addRow(tr("Hash per engine:"), m_hash);

    auto *eloRow = new QHBoxLayout;
    m_limit = new QCheckBox(tr("Limit to"), condBox);
    m_limit->setChecked(s.value(QStringLiteral("limit"), false).toBool());
    m_limit->setToolTip(tr("Engines with a strength setting (UCI_Elo or Skill Level) play at about this "
                           "rating, the others at full strength. Without the limit all play at full "
                           "strength."));
    m_elo = new QSpinBox(condBox);
    m_elo->setRange(800, 3000);
    m_elo->setSingleStep(100);
    m_elo->setSuffix(tr(" Elo"));
    m_elo->setValue(s.value(QStringLiteral("elo"), 2000).toInt());
    m_elo->setEnabled(m_limit->isChecked());
    connect(m_limit, &QCheckBox::toggled, m_elo, &QWidget::setEnabled);
    eloRow->addWidget(m_limit);
    eloRow->addWidget(m_elo);
    eloRow->addStretch();
    form->addRow(tr("Playing strength:"), eloRow);
    m_tablebases = new QCheckBox(tr("Endgame tablebases for the engines that can use them"), condBox);
    m_tablebases->setToolTip(tr("Off by default: the tables help only some engines (Stockfish, Arasan, "
                                "Viridithas, Crafty), which makes the comparison less fair."));
    m_tablebases->setChecked(s.value(QStringLiteral("tablebases"), false).toBool());
    m_tablebases->setEnabled(!tablebases::activePath().isEmpty());
    if (!m_tablebases->isEnabled())
        m_tablebases->setChecked(false);
    form->addRow(QString(), m_tablebases);
    auto *fair = new QLabel(tr("All engines: same time, threads and hash, no own opening book, no pondering."),
                            condBox);
    fair->setWordWrap(true);
    form->addRow(fair);
    layout->addWidget(condBox);

    m_follow = new QCheckBox(tr("&Follow the games on the board (clocks and evaluation included)"), this);
    m_follow->setToolTip(tr("The board shows the running game; you cannot make moves meanwhile. "
                            "Can also be switched in the menu File > Benchmarks."));
    m_follow->setChecked(s.value(QStringLiteral("follow"), true).toBool());
    layout->addWidget(m_follow);
    m_showResults = new QCheckBox(tr("Show the &results when the benchmark is complete"), this);
    m_showResults->setToolTip(tr("Opens the results window after the last game. A cancelled benchmark does not open it."));
    m_showResults->setChecked(s.value(QStringLiteral("showResults"), true).toBool());
    layout->addWidget(m_showResults);

    m_estimate = new QLabel(this);
    m_estimate->setWordWrap(true);
    layout->addWidget(m_estimate);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Cancel, this);
    m_startButton = buttons->addButton(tr("&Start benchmark"), QDialogButtonBox::AcceptRole);
    m_startButton->setDefault(true);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    layout->addWidget(buttons);

    connect(m_list, &QListWidget::itemChanged, this, &BenchmarkDialog::updateEstimate);
    for (QSpinBox *sb : { m_minutes, m_increment, m_games })
        connect(sb, QOverload<int>::of(&QSpinBox::valueChanged), this, &BenchmarkDialog::updateEstimate);
    updateEstimate();
    resize(520, 600);
}

void BenchmarkDialog::setAllChecked(bool checked)
{
    for (int i = 0; i < m_list->count(); ++i)
        m_list->item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

BenchmarkSettings BenchmarkDialog::settings() const
{
    BenchmarkSettings s;
    for (int i = 0; i < m_list->count(); ++i)
        if (m_list->item(i)->checkState() == Qt::Checked)
            s.engines << m_engines.at(i);
    s.baseMs = qint64(m_minutes->value()) * 60 * 1000;
    s.incrementMs = qint64(m_increment->value()) * 1000;
    s.gamesPerPairing = m_games->value();
    s.threads = m_threads->value();
    s.hashMb = m_hash->value();
    s.elo = m_limit->isChecked() ? m_elo->value() : 0;
    if (m_tablebases->isChecked())
        s.tablebasePath = tablebases::activePath();
    s.followOnBoard = m_follow->isChecked();
    s.showResultsAtEnd = m_showResults->isChecked();
    return s;
}

QString BenchmarkDialog::durationText(qint64 ms)
{
    const qint64 minutes = (ms + 59999) / 60000;
    if (minutes < 60)
        return tr("%n min", nullptr, int(minutes));
    const qint64 h = minutes / 60, m = minutes % 60;
    if (h >= 48)
        return tr("%1 days %2 h").arg(h / 24).arg(h % 24);
    return m ? tr("%1 h %2 min").arg(h).arg(m) : tr("%1 h").arg(h);
}

// "14:30" today, otherwise "Tue 24.11. 02:10" (local formats)
QString BenchmarkDialog::endTimeText(qint64 ms)
{
    const QDateTime end = QDateTime::currentDateTime().addMSecs(ms);
    QString format = QLocale().timeFormat(QLocale::ShortFormat);
    format.remove(QStringLiteral(":ss")).remove(QStringLiteral(".ss")); // minutes are enough
    const QString time = QLocale().toString(end.time(), format);
    if (end.date() == QDate::currentDate())
        return time;
    return QLocale().toString(end.date(), QLocale::ShortFormat) + QLatin1Char(' ') + time;
}

void BenchmarkDialog::updateEstimate()
{
    const BenchmarkSettings s = settings();
    const int n = int(s.engines.size());
    m_startButton->setEnabled(n >= 2);
    if (n < 2) {
        m_estimate->setText(tr("Choose at least two engines."));
        return;
    }
    const qint64 ms = BenchmarkRunner::estimatedMs(s);
    m_estimate->setText(tr("%1 engines, %2 games, takes up to about <b>%3</b> (finished about %4 if started now).")
                            .arg(n)
                            .arg(BenchmarkRunner::gameCount(s))
                            .arg(durationText(ms), endTimeText(ms)));
}

void BenchmarkDialog::accept()
{
    QSettings s;
    s.beginGroup(QStringLiteral("benchmark"));
    s.setValue(QStringLiteral("minutes"), m_minutes->value());
    s.setValue(QStringLiteral("increment"), m_increment->value());
    s.setValue(QStringLiteral("games"), m_games->value());
    s.setValue(QStringLiteral("threads"), m_threads->value());
    s.setValue(QStringLiteral("hash"), m_hash->value());
    s.setValue(QStringLiteral("limit"), m_limit->isChecked());
    s.setValue(QStringLiteral("elo"), m_elo->value());
    if (m_tablebases->isEnabled())
        s.setValue(QStringLiteral("tablebases"), m_tablebases->isChecked());
    s.setValue(QStringLiteral("follow"), m_follow->isChecked());
    s.setValue(QStringLiteral("showResults"), m_showResults->isChecked());
    QDialog::accept();
}
