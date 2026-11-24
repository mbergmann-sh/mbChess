// SPDX-License-Identifier: GPL-3.0-or-later
#include "engineanalysis.h"

#include "tablebases.h"

#include <chesscore/notation.h>

using namespace mbc;
using namespace mbc::engine;

EngineAnalysis::EngineAnalysis(QObject *parent)
    : QObject(parent)
{
}

EngineAnalysis::~EngineAnalysis()
{
    stop();
}

int EngineAnalysis::effectiveLines() const
{
    if (m_engine && m_ready && !m_engine->hasOption(QStringLiteral("MultiPV")))
        return 1;
    return m_multiPv;
}

bool EngineAnalysis::isStarting() const
{
    return m_running && !m_ready;
}

void EngineAnalysis::start(const EngineEntry &engine, int threads, int hashMb)
{
    QString setup = engine.path + QStringLiteral("|%1|%2").arg(threads).arg(hashMb);
    for (auto it = engine.options.cbegin(); it != engine.options.cend(); ++it)
        setup += QLatin1Char('|') + it.key() + QLatin1Char('=') + it.value();
    setup += QStringLiteral("|tb=") + tablebases::activePath() + QStringLiteral("|gtb=") + tablebases::gaviotaActivePath();
    if (m_running && setup == m_setup)
        return;
    stop();
    m_entry = engine;
    m_name = engine.name;
    m_setup = setup;
    m_threads = threads;
    m_hashMb = hashMb;
    m_running = true;
    m_ready = false;
    m_lines.clear();
    m_engine = new UciEngine(this);
    connect(m_engine, &UciEngine::optionsKnown, this, &EngineAnalysis::applyOptions);
    connect(m_engine, &UciEngine::ready, this, &EngineAnalysis::onReady);
    connect(m_engine, &UciEngine::infoReceived, this, &EngineAnalysis::onInfo);
    connect(m_engine, &UciEngine::errorOccurred, this, [this](const QString &message) {
        stop();
        emit errorOccurred(message);
    });
    m_engine->start(engine.path, engine.arguments, engine.protocol);
    emit updated();
}

void EngineAnalysis::stop()
{
    if (m_engine) {
        m_engine->disconnect(this);
        m_engine->quit();
        m_engine->deleteLater();
        m_engine = nullptr;
    }
    m_running = false;
    m_ready = false;
    m_setup.clear();
    m_lines.clear();
}

void EngineAnalysis::applyOptions()
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
    set("MultiPV", QString::number(m_multiPv));
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

void EngineAnalysis::onReady()
{
    if (m_ready)
        return;
    m_ready = true;
    m_engine->newGame();
    search();
}

void EngineAnalysis::setMultiPv(int lines)
{
    lines = qBound(1, lines, 5);
    if (lines == m_multiPv)
        return;
    m_multiPv = lines;
    if (m_engine && m_ready) {
        m_engine->stop(true);
        if (m_engine->hasOption(QStringLiteral("MultiPV")))
            m_engine->setOption(QStringLiteral("MultiPV"), QString::number(m_multiPv));
        search();
    }
}

void EngineAnalysis::analyse(const Position &startPosition, const QStringList &uciMoves)
{
    if (startPosition.toFen() == m_startPosition.toFen() && uciMoves == m_moves && !m_lines.isEmpty())
        return; // the same position: let the engine go on
    m_startPosition = startPosition;
    m_moves = uciMoves;
    search();
}

void EngineAnalysis::search()
{
    // the position analysed
    Position pos = m_startPosition;
    for (const QString &u : m_moves) {
        const Move m = parseUci(pos, u);
        if (m.isNull())
            break;
        pos.makeMove(m);
    }
    m_position = pos;
    m_lines.clear();
    emit updated();
    if (!m_engine || !m_ready)
        return; // onReady() starts the search
    if (pos.legalMoves().empty()) {
        m_engine->stop(true); // mate or stalemate: nothing to search
        return;
    }
    const QString startFen = m_startPosition.toFen() == QLatin1String(Position::StartFen)
        ? QString() : m_startPosition.toFen();
    GoParameters p;
    p.infinite = true;
    m_engine->go(startFen, m_moves, p);
}

void EngineAnalysis::onInfo(const SearchInfo &info)
{
    if (!info.hasScore || info.pv.isEmpty() || info.multiPv < 1 || info.multiPv > m_multiPv)
        return;
    if (info.lowerBound || info.upperBound)
        return; // not exact yet
    const int index = info.multiPv - 1;
    if (m_lines.size() <= index)
        m_lines.resize(index + 1);
    Line &l = m_lines[index];
    const int sign = m_position.sideToMove() == White ? 1 : -1; // UCI: from the engine's view
    l.depth = info.depth;
    l.eval.valid = true;
    l.eval.mate = info.isMate;
    l.eval.value = info.score * sign;
    l.eval.depth = info.depth;
    l.pv = info.pv;
    l.nps = info.nps;
    l.nodes = info.nodes;
    emit updated();
}
