// SPDX-License-Identifier: GPL-3.0-or-later
#include "matesearchdialog.h"

#include "tablebases.h"

#include <chesscore/notation.h>

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QTextBrowser>
#include <QTimer>
#include <QVBoxLayout>

using namespace mbc;
using namespace mbc::engine;

namespace {

bool isStockfish(const EngineEntry &e)
{
    return e.name.contains(QLatin1String("stockfish"), Qt::CaseInsensitive)
        || e.defaultName.contains(QLatin1String("stockfish"), Qt::CaseInsensitive);
}

bool isMateFor(const SearchInfo &info, int maxMoves)
{
    return info.isMate && info.score > 0 && info.score <= maxMoves;
}

} // namespace

MateSearchDialog::MateSearchDialog(const Position &position, const QString &pieceLetters, int threads,
                                   int hashMb, bool canInsert, QWidget *parent)
    : QDialog(parent)
    , m_position(position)
    , m_letters(pieceLetters)
    , m_threads(threads)
    , m_hashMb(hashMb)
    , m_canInsert(canInsert)
{
    setWindowTitle(tr("Mate search"));
    QSettings s;

    auto *info = new QLabel(this);
    info->setWordWrap(true);
    info->setText(tr("%1 to move. The engine looks for a mate in at most the given number of moves.")
                      .arg(position.sideToMove() == White ? tr("White") : tr("Black")));

    m_engineCombo = new QComboBox(this);
    m_engines = enginemanager::engines();
    const QString saved = s.value(QStringLiteral("mate/engine")).toString();
    int select = -1;
    for (int i = 0; i < m_engines.size(); ++i) {
        m_engineCombo->addItem(m_engines.at(i).name, m_engines.at(i).path);
        if (m_engines.at(i).path == saved)
            select = i;
    }
    if (select < 0) // preferably Stockfish
        for (int i = 0; i < m_engines.size() && select < 0; ++i)
            if (isStockfish(m_engines.at(i)))
                select = i;
    m_engineCombo->setCurrentIndex(qMax(0, select));

    m_moves = new QSpinBox(this);
    m_moves->setRange(1, 30);
    m_moves->setValue(qBound(1, s.value(QStringLiteral("mate/moves"), 3).toInt(), 30));
    m_moves->setSuffix(tr(" moves"));
    m_time = new QSpinBox(this);
    m_time->setRange(5, 3600);
    m_time->setSingleStep(15);
    m_time->setValue(qBound(5, s.value(QStringLiteral("mate/seconds"), 60).toInt(), 3600));
    m_time->setSuffix(tr(" s"));
    m_duals = new QCheckBox(tr("Look for &other solutions too (duals)"), this);
    m_duals->setChecked(s.value(QStringLiteral("mate/duals"), true).toBool());
    m_duals->setToolTip(tr("The engine searches several lines at once. Slower, and only engines "
                           "with the option MultiPV can do it."));

    auto *form = new QFormLayout;
    form->addRow(tr("&Engine:"), m_engineCombo);
    form->addRow(tr("&Mate in at most:"), m_moves);
    form->addRow(tr("&Time limit:"), m_time);
    form->addRow(QString(), m_duals);

    m_startButton = new QPushButton(tr("&Start search"), this);
    m_startButton->setDefault(true);
    connect(m_startButton, &QPushButton::clicked, this, [this] {
        if (m_running)
            finish(End::Cancelled);
        else
            start();
    });
    m_progress = new QLabel(this);
    m_progress->setTextInteractionFlags(Qt::TextSelectableByMouse);
    auto *startRow = new QHBoxLayout;
    startRow->addWidget(m_startButton);
    startRow->addWidget(m_progress, 1);

    m_result = new QTextBrowser(this);
    m_result->setMinimumHeight(150);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    m_insert = buttons->addButton(tr("&Insert as variation"), QDialogButtonBox::ActionRole);
    m_insert->setEnabled(false);
    m_insert->setVisible(canInsert);
    connect(m_insert, &QPushButton::clicked, this, [this] {
        m_solution = m_bestPv;
        m_solutionComment = m_bestComment;
        quitEngine();
        accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, this, &MateSearchDialog::reject);

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(info);
    layout->addLayout(form);
    layout->addLayout(startRow);
    layout->addWidget(m_result, 1);
    layout->addWidget(buttons);

    m_ticker = new QTimer(this);
    m_ticker->setInterval(250);
    connect(m_ticker, &QTimer::timeout, this, &MateSearchDialog::onTick);

    if (m_engines.isEmpty()) {
        m_startButton->setEnabled(false);
        m_result->setPlainText(tr("There is no engine (menu Engines > Manage engines)."));
    } else if (position.legalMoves().empty()) {
        m_startButton->setEnabled(false);
        m_result->setPlainText(position.inCheck() ? tr("The position is already checkmate.")
                                                  : tr("The position is stalemate: no moves possible."));
    }
    resize(560, 440);
}

MateSearchDialog::~MateSearchDialog()
{
    quitEngine();
}

void MateSearchDialog::reject()
{
    quitEngine();
    QDialog::reject();
}

void MateSearchDialog::setRunning(bool running)
{
    m_running = running;
    m_startButton->setText(running ? tr("S&top search") : tr("&Start search"));
    m_engineCombo->setEnabled(!running);
    m_moves->setEnabled(!running);
    m_time->setEnabled(!running);
    m_duals->setEnabled(!running);
    if (running)
        m_ticker->start();
    else
        m_ticker->stop();
}

void MateSearchDialog::start()
{
    const int index = m_engineCombo->currentIndex();
    if (index < 0 || index >= m_engines.size())
        return;
    QSettings s;
    s.setValue(QStringLiteral("mate/engine"), m_engines.at(index).path);
    s.setValue(QStringLiteral("mate/moves"), m_moves->value());
    s.setValue(QStringLiteral("mate/seconds"), m_time->value());
    s.setValue(QStringLiteral("mate/duals"), m_duals->isChecked());

    quitEngine();
    m_entry = m_engines.at(index);
    m_target = m_moves->value();
    m_lineCount = m_duals->isChecked() ? 4 : 1;
    m_goMate = isStockfish(m_entry) && m_entry.protocol == Protocol::Uci;
    m_lines.clear();
    m_engineErrors.clear();
    m_depth = 0;
    m_nodes = m_nps = 0;
    m_foundAt = -1;
    m_foundDepth = 0;
    m_bestPv.clear();
    m_bestComment.clear();
    m_insert->setEnabled(false);
    m_result->clear();
    m_progress->setText(tr("Starting the engine..."));

    m_engine = new UciEngine(this);
    connect(m_engine, &UciEngine::optionsKnown, this, &MateSearchDialog::applyOptions);
    connect(m_engine, &UciEngine::ready, this, &MateSearchDialog::onReady);
    connect(m_engine, &UciEngine::infoReceived, this, &MateSearchDialog::onInfo);
    connect(m_engine, &UciEngine::bestMove, this, [this] {
        if (m_running)
            finish(End::EngineDone);
    });
    connect(m_engine, &UciEngine::errorOccurred, this, [this](const QString &message) {
        finish(End::Error, message);
    });
    // engines report fatal problems as "info string ERROR ..." and then exit
    connect(m_engine, &UciEngine::lineReceived, this, [this](const QString &line) {
        if (line.startsWith(QLatin1String("info string ERROR"), Qt::CaseInsensitive) && m_engineErrors.size() < 6)
            m_engineErrors << line.mid(12).trimmed();
    });
    connect(m_engine, &UciEngine::stateChanged, this, [this](UciEngine::State state) {
        if (state == UciEngine::State::NotRunning && m_running) {
            QString message = tr("The engine \"%1\" has quit.").arg(m_entry.name);
            if (!m_engineErrors.isEmpty())
                message += QLatin1Char('\n') + m_engineErrors.join(QLatin1Char('\n'));
            finish(End::Error, message);
        }
    });
    setRunning(true);
    m_clock.start();
    m_engine->start(m_entry.path, m_entry.arguments, m_entry.protocol);
}

void MateSearchDialog::applyOptions()
{
    UciEngine *e = m_engine;
    if (!e)
        return;
    for (auto it = m_entry.options.cbegin(); it != m_entry.options.cend(); ++it)
        if (e->hasOption(it.key()) && !enginemanager::isManagedOption(it.key()))
            e->setOption(it.key(), it.value());
    auto set = [e](const char *name, const QString &value) {
        if (e->hasOption(QLatin1String(name)))
            e->setOption(QLatin1String(name), value);
    };
    set("Threads", QString::number(m_threads));
    set("Cores", QString::number(m_threads));
    set("Hash", QString::number(m_hashMb));
    set("MultiPV", QString::number(m_lineCount));
    set("UCI_AnalyseMode", QStringLiteral("true"));
    set("UCI_LimitStrength", QStringLiteral("false"));
    set("OwnBook", QStringLiteral("false"));
    set("Ponder", QStringLiteral("false"));
    if (e->hasOption(QStringLiteral("Skill Level"))) {
        const EngineOption o = e->option(QStringLiteral("Skill Level"));
        e->setOption(QStringLiteral("Skill Level"), o.max.isEmpty() ? QStringLiteral("20") : o.max);
    }
    tablebases::applyToEngine(e, tablebases::activePath());
}

void MateSearchDialog::onReady()
{
    if (!m_engine || !m_running || m_engine->state() != UciEngine::State::Ready)
        return;
    if (!m_engine->hasOption(QStringLiteral("MultiPV")))
        m_lineCount = 1;
    m_engine->newGame();
    GoParameters p;
    if (m_goMate)
        p.mate = m_target;
    else
        p.infinite = true;
    m_engine->go(m_position.toFen(), {}, p);
    m_progress->setText(tr("Searching..."));
}

void MateSearchDialog::onInfo(const SearchInfo &info)
{
    if (info.nodes > 0)
        m_nodes = info.nodes;
    if (info.nps > 0)
        m_nps = info.nps;
    if (!info.hasScore || info.pv.isEmpty() || info.lowerBound || info.upperBound)
        return;
    if (info.multiPv < 1 || info.multiPv > m_lineCount)
        return;
    m_depth = qMax(m_depth, info.depth);
    m_lines[info.multiPv] = info;
    if (m_foundAt < 0 && info.multiPv == 1 && isMateFor(info, m_target)) {
        m_foundAt = m_clock.elapsed();
        m_foundDepth = info.depth;
    }
    onTick();
}

void MateSearchDialog::onTick()
{
    if (!m_running)
        return;
    const qint64 ms = m_clock.elapsed();
    QString text = tr("%1 s, depth %2").arg(ms / 1000).arg(m_depth);
    if (m_nps > 0)
        text += QStringLiteral(", ") + tr("%1 kN/s").arg(QLocale().toString(m_nps / 1000));
    if (m_lines.contains(1))
        text += QStringLiteral(", ") + scoreText(m_lines.value(1));
    m_progress->setText(text);

    if (ms >= qint64(m_time->value()) * 1000) {
        finish(End::Timeout);
        return;
    }
    // A mate was found: search a little longer, maybe there is a shorter one (and the
    // other lines settle). "go mate" stops by itself.
    if (m_foundAt >= 0 && !m_goMate) {
        const bool deepEnough = m_depth >= m_foundDepth + 4;
        if (deepEnough || ms - m_foundAt >= 3000)
            finish(End::Found);
    }
}

QString MateSearchDialog::scoreText(const SearchInfo &info) const
{
    if (info.isMate)
        return info.score > 0 ? tr("mate in %1").arg(info.score) : tr("gets mated in %1").arg(-info.score);
    return QStringLiteral("%1%2").arg(info.score >= 0 ? QStringLiteral("+") : QString())
        .arg(info.score / 100.0, 0, 'f', 2);
}

QString MateSearchDialog::sanLine(const QStringList &pv, int maxPlies) const
{
    QStringList moves;
    Position pos = m_position;
    for (int i = 0; i < pv.size() && i < maxPlies; ++i) {
        const Move m = parseUci(pos, pv.at(i));
        if (m.isNull())
            break;
        QString san = localizeSan(toSan(pos, m), m_letters);
        if (pos.sideToMove() == White)
            san = QStringLiteral("%1. ").arg(pos.fullmoveNumber()) + san;
        else if (i == 0)
            san = QStringLiteral("%1... ").arg(pos.fullmoveNumber()) + san;
        moves << san;
        pos.makeMove(m);
    }
    return moves.join(QLatin1Char(' '));
}

void MateSearchDialog::finish(End how, const QString &error)
{
    if (!m_running)
        return;
    const qint64 ms = m_clock.elapsed();
    setRunning(false);
    quitEngine();
    m_progress->setText(tr("Searched %1 s, depth %2.").arg(QLocale().toString(ms / 1000.0, 'f', 1)).arg(m_depth));

    if (how == End::Error) {
        m_result->setPlainText(tr("The engine reported an error:\n%1").arg(error));
        return;
    }
    const QString side = m_position.sideToMove() == White ? tr("White") : tr("Black");
    const QString other = m_position.sideToMove() == White ? tr("Black") : tr("White");

    // all lines that mate in <= N, shortest first; each first move once
    QVector<SearchInfo> solutions;
    for (const SearchInfo &l : m_lines)
        if (isMateFor(l, m_target))
            solutions << l;
    std::stable_sort(solutions.begin(), solutions.end(),
                     [](const SearchInfo &a, const SearchInfo &b) { return a.score < b.score; });

    QString html;
    if (!solutions.isEmpty()) {
        const SearchInfo &best = solutions.constFirst();
        const int plies = 2 * best.score - 1;
        m_bestPv = best.pv.mid(0, plies);
        m_bestComment = tr("Mate in %1 (%2)").arg(best.score).arg(m_entry.name);
        html += QStringLiteral("<p><b>%1</b></p>").arg(tr("%1 mates in %2:").arg(side).arg(best.score).toHtmlEscaped());
        html += QStringLiteral("<p style='font-size:large'>%1</p>").arg(sanLine(best.pv, plies).toHtmlEscaped());
        if (best.score < m_target)
            html += QStringLiteral("<p>%1</p>").arg(tr("That is shorter than the mate in %1 you looked for.").arg(m_target).toHtmlEscaped());
        if (solutions.size() > 1) {
            QStringList rows;
            for (int i = 1; i < solutions.size(); ++i)
                rows << QStringLiteral("<li>%1 &nbsp;(%2)</li>")
                            .arg(sanLine(solutions.at(i).pv, 2 * solutions.at(i).score - 1).toHtmlEscaped(),
                                 tr("mate in %1").arg(solutions.at(i).score).toHtmlEscaped());
            html += QStringLiteral("<p>%1</p><ul>%2</ul>")
                        .arg(tr("Other solutions (duals), %n:", nullptr, solutions.size() - 1).toHtmlEscaped(),
                             rows.join(QString()));
        } else if (m_lineCount > 1) {
            html += QStringLiteral("<p>%1</p>").arg(tr("No dual: none of the next %n best moves mates as well.", nullptr, m_lineCount - 1).toHtmlEscaped());
        } else {
            html += QStringLiteral("<p>%1</p>").arg(tr("Other solutions were not looked for.").toHtmlEscaped());
        }
        if (how == End::Cancelled || how == End::Timeout)
            html += QStringLiteral("<p><i>%1</i></p>").arg(tr("The search was stopped early: a shorter mate may exist.").toHtmlEscaped());
    } else {
        const SearchInfo best = m_lines.value(1);
        if (best.isMate && best.score > 0) {
            m_bestPv = best.pv.mid(0, 2 * best.score - 1);
            m_bestComment = tr("Mate in %1 (%2)").arg(best.score).arg(m_entry.name);
            html += QStringLiteral("<p><b>%1</b></p>").arg(tr("No mate in %n move(s) found.", nullptr, m_target).toHtmlEscaped());
            html += QStringLiteral("<p>%1</p><p>%2</p>")
                        .arg(tr("But %1 mates in %2:").arg(side).arg(best.score).toHtmlEscaped(),
                             sanLine(best.pv, 2 * best.score - 1).toHtmlEscaped());
        } else if (best.isMate && best.score < 0) {
            html += QStringLiteral("<p><b>%1</b></p><p>%2</p>")
                        .arg(tr("No mate - on the contrary: %1 mates in %2.").arg(other).arg(-best.score).toHtmlEscaped(),
                             sanLine(best.pv, 2 * -best.score).toHtmlEscaped());
        } else {
            QString why = how == End::Timeout ? tr("within the time limit") : tr("so far");
            html += QStringLiteral("<p><b>%1</b></p>")
                        .arg(tr("No mate in %n move(s) found %1.", nullptr, m_target).arg(why).toHtmlEscaped());
            if (m_lines.contains(1))
                html += QStringLiteral("<p>%1</p>")
                            .arg(tr("Evaluation %1 (depth %2): %3")
                                     .arg(scoreText(best)).arg(best.depth).arg(sanLine(best.pv, 8)).toHtmlEscaped());
            if (how == End::Timeout)
                html += QStringLiteral("<p>%1</p>").arg(tr("A longer time limit may still find one.").toHtmlEscaped());
        }
    }
    m_result->setHtml(html);
    m_insert->setEnabled(m_canInsert && !m_bestPv.isEmpty());
}

void MateSearchDialog::quitEngine()
{
    if (!m_engine)
        return;
    m_engine->disconnect(this);
    m_engine->quit();
    m_engine->deleteLater();
    m_engine = nullptr;
}
