// SPDX-License-Identifier: GPL-3.0-or-later
#include "tutor.h"

#include "engineanalysis.h"
#include "enginemanager.h"

#include <chesscore/notation.h>

#include <QCoreApplication>
#include <QSettings>
#include <QTimer>
#include <QtMath>

using namespace mbc;

namespace {

const int kBeforeMs = 1000;    // search of the position before the move, at least
const int kAfterMs = 1000;     // search of the position after the move
const int kGiveUpMs = 10000;   // the engine does not answer: the move is played unrated
const int kLines = 3;

// Centipawns from White's view; mates as very large values
int centipawns(const Evaluation &e)
{
    if (!e.mate)
        return e.value;
    if (e.value == 0)
        return 0;
    return e.value > 0 ? 100000 - 100 * e.value : -100000 - 100 * e.value;
}

} // namespace

Tutor::Tutor(QObject *parent)
    : QObject(parent)
    , m_analysis(new EngineAnalysis(this))
    , m_poll(new QTimer(this))
{
    m_analysis->setMultiPv(kLines);
    m_poll->setInterval(100);
    connect(m_poll, &QTimer::timeout, this, &Tutor::poll);
    connect(m_analysis, &EngineAnalysis::errorOccurred, this, [this](const QString &message) {
        const bool waiting = isChecking();
        if (waiting)
            finish(); // the move is played unrated
        emit errorOccurred(message);
    });
}

Tutor::~Tutor() = default;

bool Tutor::isEnabled()
{
    return QSettings().value(QStringLiteral("tutor/enabled"), false).toBool();
}

void Tutor::setEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("tutor/enabled"), on);
}

Tutor::Severity Tutor::level()
{
    return Severity(qBound(int(Inaccuracy), QSettings().value(QStringLiteral("tutor/level"), int(Mistake)).toInt(),
                           int(Blunder)));
}

void Tutor::setLevel(Severity s)
{
    QSettings().setValue(QStringLiteral("tutor/level"), int(s));
}

QString Tutor::severityName(Severity s)
{
    switch (s) {
    case Good: return QCoreApplication::translate("Tutor", "Good move");
    case Inaccuracy: return QCoreApplication::translate("Tutor", "Inaccuracy");
    case Mistake: return QCoreApplication::translate("Tutor", "Mistake");
    case Blunder: return QCoreApplication::translate("Tutor", "Blunder");
    case Stoned: return QCoreApplication::translate("Tutor", "Terrible blunder");
    }
    return QString();
}

double Tutor::winChance(const Evaluation &e, Color side)
{
    if (!e.valid)
        return 50;
    double cp = centipawns(e);
    if (side == Black)
        cp = -cp;
    if (cp > 50000)
        return 100;
    if (cp < -50000)
        return 0;
    // Lichess: winning chances from centipawns
    return 50 + 50 * (2 / (1 + qExp(-0.00368208 * cp)) - 1);
}

void Tutor::setPosition(bool active, const EngineEntry &engine, const Position &start, const QStringList &moves)
{
    if (isChecking())
        return; // the window waits for the verdict
    m_active = active;
    if (!active) {
        if (m_analysis->isRunning())
            m_analysis->pause();
        m_searching.clear();
        return;
    }
    m_analysis->start(engine, 1, 64);
    m_start = start;
    m_moves = moves;
    const QString key = start.toFen() + QLatin1Char('|') + moves.join(QLatin1Char(' '));
    if (key != m_searching || m_analysis->lines().isEmpty())
        m_searchClock.start();
    m_searching = key;
    m_analysis->analyse(start, moves);
}

void Tutor::stop()
{
    cancel();
    m_active = false;
    m_analysis->stop();
    m_moves.clear();
    m_start = Position();
    m_searching.clear();
}

void Tutor::cancel()
{
    m_poll->stop();
    m_phase = Phase::Idle;
}

void Tutor::check(Move move)
{
    Position pos = m_start;
    for (const QString &u : m_moves)
        pos.makeMove(parseUci(pos, u));
    m_position = pos;
    m_verdict = Verdict();
    m_verdict.move = move;
    m_verdict.fen = pos.toFen();
    m_phase = Phase::Before;
    m_waitClock.start();
    if (!m_active || !m_analysis->isRunning()) {
        finish();
        return;
    }
    const Position after = pos.after(move);
    if (after.isCheckmate()) { // nothing to improve
        finish();
        return;
    }
    m_poll->start();
    poll();
}

bool Tutor::lineUsable(int index) const
{
    const QVector<EngineAnalysis::Line> &lines = m_analysis->lines();
    return index < lines.size() && lines.at(index).eval.valid && !lines.at(index).pv.isEmpty()
        && lines.at(index).depth >= lines.constFirst().depth - 2;
}

void Tutor::poll()
{
    if (m_phase == Phase::Idle)
        return;
    if (m_waitClock.elapsed() > kGiveUpMs) {
        m_verdict.severity = Good; // no answer: the move is played unrated
        m_verdict.best = Move();
        finish();
        return;
    }
    const QVector<EngineAnalysis::Line> &lines = m_analysis->lines();
    if (m_phase == Phase::Before) {
        if (lines.isEmpty() || !lineUsable(0) || m_searchClock.elapsed() < kBeforeMs)
            return;
        m_verdict.before = lines.constFirst().eval;
        m_verdict.bestLine = lines.constFirst().pv;
        m_verdict.best = parseUci(m_position, m_verdict.bestLine.constFirst());
        const QString uci = toUci(m_verdict.move);
        for (int i = 0; i < lines.size(); ++i) {
            if (lineUsable(i) && lines.at(i).pv.constFirst() == uci) {
                // one of the tutor's own lines
                m_verdict.after = lines.at(i).eval;
                m_verdict.reply = lines.at(i).pv.mid(1);
                finish();
                return;
            }
        }
        const Position after = m_position.after(m_verdict.move);
        if (after.legalMoves().empty()) { // stalemate (mate was handled in check())
            m_verdict.after.valid = true;
            m_verdict.after.value = 0;
            finish();
            return;
        }
        m_phase = Phase::After;
        m_searchClock.start();
        m_searching.clear();
        m_analysis->analyse(m_start, m_moves + QStringList { uci });
        return;
    }
    // After
    if (lines.isEmpty() || !lineUsable(0) || m_searchClock.elapsed() < kAfterMs)
        return;
    m_verdict.after = lines.constFirst().eval;
    m_verdict.reply = lines.constFirst().pv;
    finish();
}

void Tutor::finish()
{
    m_poll->stop();
    m_phase = Phase::Idle;
    Verdict &v = m_verdict;
    if (v.before.valid && v.after.valid && !v.best.isNull() && !(v.best == v.move)) {
        const Color side = m_position.sideToMove();
        v.drop = qMax(0.0, winChance(v.before, side) - winChance(v.after, side));
        // Lichess: 0.1 / 0.2 / 0.3 of the winning chances from -1 to 1, here 0..100
        v.severity = v.drop >= 35 ? Stoned : v.drop >= 15 ? Blunder : v.drop >= 10 ? Mistake
                                                                     : v.drop >= 5 ? Inaccuracy : Good;
        // A particularly stupid move also in a position that was already lost (or still
        // won): the queen (6 pawns or more) or a mate given away. The winning chances then
        // fall less, because they were low (high) before.
        if (v.severity >= Mistake && v.severity < Stoned) {
            const int sign = side == White ? 1 : -1;
            const int cpBefore = qBound(-2000, centipawns(v.before) * sign, 2000);
            const int cpAfter = qBound(-2000, centipawns(v.after) * sign, 2000);
            const bool mateGiven = v.after.mate && v.after.value * sign < 0 && !(v.before.mate && v.before.value * sign < 0);
            if (mateGiven || cpBefore - cpAfter >= 600)
                v.severity = Stoned;
        }
    } else {
        v.severity = Good;
    }
    m_active = false; // the next setPosition() decides
    emit verdictReady(v);
}
