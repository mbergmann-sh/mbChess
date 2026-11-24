// SPDX-License-Identifier: GPL-3.0-or-later
#include "benchmarkwindow.h"

#include "benchmarkdialog.h"

#include <pgn/pgn.h>

#include <QDate>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSettings>
#include <QSplitter>
#include <QTreeWidget>
#include <QVBoxLayout>

using namespace mbc;

namespace {

// Sorts by the value in Qt::UserRole when there is one (numbers, durations), else by text.
class SortItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;
    bool operator<(const QTreeWidgetItem &other) const override
    {
        const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        const QVariant a = data(col, Qt::UserRole), b = other.data(col, Qt::UserRole);
        if (a.isValid() && b.isValid())
            return a.toDouble() < b.toDouble();
        return text(col).localeAwareCompare(other.text(col)) < 0;
    }
};

QString clockText(qint64 ms)
{
    const qint64 s = ms / 1000;
    if (s >= 3600)
        return QStringLiteral("%1:%2:%3").arg(s / 3600).arg(s / 60 % 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QLatin1Char('0'));
}

QString pointsText(double p)
{
    return QString::number(p, 'f', p == int(p) ? 0 : 1);
}

struct Standing
{
    int engine = -1;
    double points = 0;
    double sb = 0;         // Sonneborn-Berger
    int games = 0, wins = 0, draws = 0, losses = 0;
    qint64 thinkMs = 0;
    int moves = 0;
};

} // namespace

BenchmarkWindow::BenchmarkWindow(BenchmarkRunner *runner, QWidget *parent)
    : QWidget(parent, Qt::Window)
    , m_runner(runner)
{
    setAttribute(Qt::WA_DeleteOnClose, false);
    auto *layout = new QVBoxLayout(this);
    m_summary = new QLabel(this);
    m_summary->setWordWrap(true);
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_summary);

    auto *splitter = new QSplitter(Qt::Vertical, this);
    auto *top = new QWidget(splitter);
    auto *topLayout = new QVBoxLayout(top);
    topLayout->setContentsMargins(0, 0, 0, 0);
    m_standingsLabel = new QLabel(top);
    topLayout->addWidget(m_standingsLabel);
    m_standings = new QTreeWidget(top);
    m_standings->setRootIsDecorated(false);
    m_standings->setAlternatingRowColors(true);
    m_standings->setSortingEnabled(true);
    m_standings->setUniformRowHeights(true);
    m_standings->sortByColumn(0, Qt::AscendingOrder);
    m_standings->header()->setStretchLastSection(false);
    topLayout->addWidget(m_standings);

    auto *bottom = new QWidget(splitter);
    auto *bottomLayout = new QVBoxLayout(bottom);
    bottomLayout->setContentsMargins(0, 0, 0, 0);
    m_gamesLabel = new QLabel(bottom);
    bottomLayout->addWidget(m_gamesLabel);
    m_games = new QTreeWidget(bottom);
    m_games->setRootIsDecorated(false);
    m_games->setAlternatingRowColors(true);
    m_games->setSortingEnabled(true);
    m_games->setUniformRowHeights(true);
    m_games->sortByColumn(0, Qt::AscendingOrder);
    m_games->header()->setStretchLastSection(false);
    bottomLayout->addWidget(m_games);
    splitter->addWidget(top);
    splitter->addWidget(bottom);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 2);
    layout->addWidget(splitter, 1);

    auto *buttons = new QHBoxLayout;
    m_saveButton = new QPushButton(this);
    m_closeButton = new QPushButton(this);
    connect(m_saveButton, &QPushButton::clicked, this, &BenchmarkWindow::saveGames);
    connect(m_closeButton, &QPushButton::clicked, this, &QWidget::close);
    buttons->addWidget(m_saveButton);
    buttons->addStretch();
    buttons->addWidget(m_closeButton);
    layout->addLayout(buttons);

    connect(runner, &BenchmarkRunner::gameStarted, this, [this] { m_status = 0; refresh(); });
    connect(runner, &BenchmarkRunner::gameFinished, this, &BenchmarkWindow::refresh);
    connect(runner, &BenchmarkRunner::finished, this, [this](bool cancelled) {
        m_status = cancelled ? 2 : 1;
        refresh();
    });

    retranslateUi();
    resize(900, 640);
}

void BenchmarkWindow::showEvent(QShowEvent *event)
{
    refresh(); // up to date whenever it is opened, also while the benchmark runs
    QWidget::showEvent(event);
}

void BenchmarkWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange)
        retranslateUi();
    QWidget::changeEvent(event);
}

void BenchmarkWindow::retranslateUi()
{
    setWindowTitle(tr("Benchmark results"));
    m_standingsLabel->setText(tr("<b>Standings</b>"));
    m_gamesLabel->setText(tr("<b>Games</b>"));
    m_standings->setHeaderLabels({ tr("Rank"), tr("Engine"), tr("Points"), tr("Games"), tr("Won"), tr("Drawn"),
                                   tr("Lost"), tr("Score"), tr("Sonneborn-Berger"), tr("Avg. time per move") });
    m_games->setHeaderLabels({ tr("No."), tr("White"), tr("Black"), tr("Result"), tr("Reason"), tr("Opening"),
                               tr("Moves"), tr("Duration"), tr("Time White"), tr("Time Black") });
    m_saveButton->setText(tr("&Save games as PGN..."));
    m_closeButton->setText(tr("&Close"));
    refresh();
}

void BenchmarkWindow::refresh()
{
    const BenchmarkSettings &set = m_runner->settings();
    const QVector<BenchmarkGame> &games = m_runner->games();
    const int n = int(set.engines.size());

    // --- standings
    QVector<Standing> st(n);
    for (int i = 0; i < n; ++i)
        st[i].engine = i;
    for (const BenchmarkGame &g : games) {
        const double w = g.result == Result::WhiteWins ? 1 : g.result == Result::Draw ? 0.5 : 0;
        Standing &sw = st[g.white];
        Standing &sb = st[g.black];
        sw.points += w;
        sb.points += 1 - w;
        ++sw.games;
        ++sb.games;
        if (g.result == Result::WhiteWins) {
            ++sw.wins;
            ++sb.losses;
        } else if (g.result == Result::BlackWins) {
            ++sb.wins;
            ++sw.losses;
        } else {
            ++sw.draws;
            ++sb.draws;
        }
        sw.thinkMs += g.thinkMs[0];
        sb.thinkMs += g.thinkMs[1];
        sw.moves += g.moves[0];
        sb.moves += g.moves[1];
    }
    for (const BenchmarkGame &g : games) {
        const double w = g.result == Result::WhiteWins ? 1 : g.result == Result::Draw ? 0.5 : 0;
        st[g.white].sb += w * st[g.black].points;
        st[g.black].sb += (1 - w) * st[g.white].points;
    }
    std::stable_sort(st.begin(), st.end(), [](const Standing &a, const Standing &b) {
        if (a.points != b.points)
            return a.points > b.points;
        if (a.sb != b.sb)
            return a.sb > b.sb;
        return a.wins > b.wins;
    });

    m_standings->setSortingEnabled(false);
    m_standings->clear();
    const bool anyGame = !games.isEmpty();
    for (int r = 0; r < n; ++r) {
        const Standing &s = st[r];
        const int rank = r > 0 && st[r - 1].points == s.points && st[r - 1].sb == s.sb ? -1 : r + 1;
        auto *item = new SortItem(m_standings);
        const bool winner = anyGame && r == 0 && (n < 2 || st[1].points != s.points || st[1].sb != s.sb);
        QString name = set.engines.at(s.engine).name;
        item->setText(0, rank > 0 ? QString::number(rank) : QString());
        item->setData(0, Qt::UserRole, r + 1);
        item->setText(1, winner ? QStringLiteral("★ ") + name : name);
        const double vals[] = { s.points, double(s.games), double(s.wins), double(s.draws), double(s.losses) };
        for (int c = 0; c < 5; ++c) {
            item->setText(2 + c, c == 0 ? pointsText(vals[c]) : QString::number(int(vals[c])));
            item->setData(2 + c, Qt::UserRole, vals[c]);
            item->setTextAlignment(2 + c, Qt::AlignRight | Qt::AlignVCenter);
        }
        const double pct = s.games ? 100.0 * s.points / s.games : 0;
        item->setText(7, s.games ? QStringLiteral("%1 %").arg(pct, 0, 'f', 1) : QString());
        item->setData(7, Qt::UserRole, pct);
        item->setText(8, pointsText(s.sb));
        item->setData(8, Qt::UserRole, s.sb);
        const double avg = s.moves ? double(s.thinkMs) / s.moves / 1000.0 : 0;
        item->setText(9, s.moves ? tr("%1 s").arg(avg, 0, 'f', 2) : QString());
        item->setData(9, Qt::UserRole, avg);
        for (int c = 7; c <= 9; ++c)
            item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        if (winner) {
            QFont f = item->font(0);
            f.setBold(true);
            const QBrush gold(QColor(255, 200, 0, 110));
            for (int c = 0; c < m_standings->columnCount(); ++c) {
                item->setFont(c, f);
                item->setBackground(c, gold);
            }
            item->setToolTip(1, tr("Winner of the benchmark"));
        }
    }
    m_standings->setSortingEnabled(true);
    m_standings->sortByColumn(m_standings->header()->sortIndicatorSection(), m_standings->header()->sortIndicatorOrder());
    for (int c = 0; c < m_standings->columnCount(); ++c)
        m_standings->resizeColumnToContents(c);

    // --- games
    m_games->setSortingEnabled(false);
    m_games->clear();
    qint64 total = 0;
    for (int i = 0; i < games.size(); ++i) {
        const BenchmarkGame &g = games.at(i);
        total += g.durationMs;
        auto *item = new SortItem(m_games);
        item->setText(0, QString::number(i + 1));
        item->setData(0, Qt::UserRole, i + 1);
        item->setText(1, set.engines.at(g.white).name);
        item->setText(2, set.engines.at(g.black).name);
        item->setText(3, resultToString(g.result));
        item->setText(4, g.reason);
        item->setText(5, g.opening);
        const int moves = (g.plies + 1) / 2;
        item->setText(6, QString::number(moves));
        item->setData(6, Qt::UserRole, moves);
        item->setText(7, clockText(g.durationMs));
        item->setData(7, Qt::UserRole, double(g.durationMs));
        item->setText(8, clockText(g.thinkMs[0]));
        item->setData(8, Qt::UserRole, double(g.thinkMs[0]));
        item->setText(9, clockText(g.thinkMs[1]));
        item->setData(9, Qt::UserRole, double(g.thinkMs[1]));
        for (int c : { 0, 3, 6, 7, 8, 9 })
            item->setTextAlignment(c, Qt::AlignRight | Qt::AlignVCenter);
        if (g.result != Result::Draw) {
            const int winCol = g.result == Result::WhiteWins ? 1 : 2;
            QFont f = item->font(winCol);
            f.setBold(true);
            item->setFont(winCol, f);
        }
    }
    m_games->setSortingEnabled(true);
    m_games->sortByColumn(m_games->header()->sortIndicatorSection(), m_games->header()->sortIndicatorOrder());
    for (int c = 0; c < m_games->columnCount(); ++c)
        m_games->resizeColumnToContents(c);

    // --- summary
    const QString status = m_status == 0 ? tr("running") : m_status == 1 ? tr("finished") : tr("cancelled");
    const qint64 remaining = m_runner->remainingMs();
    const QString expected = remaining >= 0
        ? tr("Expected time to the end: about %1 (finished about %2).")
                  .arg(BenchmarkDialog::durationText(remaining), BenchmarkDialog::endTimeText(remaining))
            + QStringLiteral("<br>")
        : QString();
    const QString strength = set.elo > 0 ? tr("limited to %1 Elo where possible").arg(set.elo) : tr("full strength");
    m_summary->setText(expected + tr("<b>Engine benchmark</b> (%1): %2 of %3 games played, playing time %4, total time %5.<br>"
                          "Time control %6 min + %7 s per move, %8 thread(s) and %9 MB hash per engine, %10, "
                          "no own books%11.")
                           .arg(status)
                           .arg(games.size())
                           .arg(m_runner->totalGames())
                           .arg(clockText(total), clockText(m_runner->elapsedMs()))
                           .arg(set.baseMs / 60000.0)
                           .arg(set.incrementMs / 1000.0)
                           .arg(set.threads)
                           .arg(set.hashMb)
                           .arg(strength)
                           .arg(set.tablebasePath.isEmpty() && set.gaviotaPath.isEmpty() ? QString() : tr(", endgame tablebases where supported")));
    m_saveButton->setEnabled(anyGame);
}

void BenchmarkWindow::saveGames()
{
    QSettings s;
    const QString dir = s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString();
    const QString name = QStringLiteral("mbChess-Benchmark %1.pgn").arg(QDate::currentDate().toString(Qt::ISODate));
    QString file = QFileDialog::getSaveFileName(this, tr("Save games"), QDir(dir).filePath(name),
                                                tr("PGN files (*.pgn)"));
    if (file.isEmpty())
        return;
    if (!file.endsWith(QLatin1String(".pgn"), Qt::CaseInsensitive))
        file += QStringLiteral(".pgn");
    s.setValue(QStringLiteral("files/lastDir"), QFileInfo(file).absolutePath());

    QString text;
    for (const BenchmarkGame &g : m_runner->games())
        text += pgn::write(g.game) + QLatin1Char('\n');
    QSaveFile out(file);
    if (!out.open(QIODevice::WriteOnly | QIODevice::Text) || out.write(text.toUtf8()) < 0 || !out.commit()) {
        QMessageBox::warning(this, tr("Save games"), tr("The games could not be saved:\n%1").arg(out.errorString()));
        return;
    }
}
