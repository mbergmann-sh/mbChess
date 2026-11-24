// SPDX-License-Identifier: GPL-3.0-or-later
#include "benchmark.h"

#include "enginemanager.h"
#include "tablebases.h"

#include <chesscore/notation.h>

#include <QCoreApplication>
#include <QDateTime>
#include <QTimer>

using namespace mbc;
using mbc::engine::GoParameters;
using mbc::engine::UciEngine;

namespace {

// Balanced, well known start lines. Each pairing plays one of them with both colours.
struct Opening
{
    const char *name;
    const char *moves;
};
const Opening Openings[] = {
    { QT_TRANSLATE_NOOP("Benchmark", "Italian Game"), "e2e4 e7e5 g1f3 b8c6 f1c4 f8c5" },
    { QT_TRANSLATE_NOOP("Benchmark", "Queen's Gambit Declined"), "d2d4 d7d5 c2c4 e7e6 b1c3 g8f6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Sicilian Defence"), "e2e4 c7c5 g1f3 d7d6 d2d4 c5d4 f3d4 g8f6 b1c3" },
    { QT_TRANSLATE_NOOP("Benchmark", "King's Indian Defence"), "d2d4 g8f6 c2c4 g7g6 b1c3 f8g7 e2e4 d7d6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Ruy Lopez"), "e2e4 e7e5 g1f3 b8c6 f1b5 a7a6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Slav Defence"), "d2d4 d7d5 c2c4 c7c6 g1f3 g8f6" },
    { QT_TRANSLATE_NOOP("Benchmark", "French Defence"), "e2e4 e7e6 d2d4 d7d5 b1c3 g8f6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Nimzo-Indian Defence"), "d2d4 g8f6 c2c4 e7e6 b1c3 f8b4" },
    { QT_TRANSLATE_NOOP("Benchmark", "Caro-Kann Defence"), "e2e4 c7c6 d2d4 d7d5 b1c3 d5e4 c3e4" },
    { QT_TRANSLATE_NOOP("Benchmark", "English Opening"), "c2c4 e7e5 b1c3 g8f6 g1f3 b8c6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Reti Opening"), "g1f3 d7d5 c2c4 e7e6 g2g3 g8f6" },
    { QT_TRANSLATE_NOOP("Benchmark", "Scandinavian Defence"), "e2e4 d7d5 e4d5 d8d5 b1c3 d5a5" },
};
constexpr int OpeningCount = int(sizeof(Openings) / sizeof(Openings[0]));

constexpr int MaxPlies = 500;            // then the game is drawn by adjudication
constexpr qint64 StartTimeoutMs = 30000; // both engines must be ready by then
constexpr qint64 FlagGraceMs = 2000;     // an engine that does not answer loses on time

QString trB(const char *text)
{
    return QCoreApplication::translate("Benchmark", text);
}

} // namespace

BenchmarkRunner::BenchmarkRunner(QObject *parent)
    : QObject(parent)
{
    m_flagTimer = new QTimer(this);
    m_flagTimer->setSingleShot(true);
    connect(m_flagTimer, &QTimer::timeout, this, &BenchmarkRunner::onTimeout);
    m_startTimer = new QTimer(this);
    m_startTimer->setSingleShot(true);
    connect(m_startTimer, &QTimer::timeout, this, [this] {
        if (!m_inGame)
            return;
        // the engine that is not ready loses (both: draw, the game does not count much)
        const bool w = m_players[0].ready, b = m_players[1].ready;
        endGame(w == b ? Result::Draw : (w ? Result::WhiteWins : Result::BlackWins),
                trB(QT_TRANSLATE_NOOP("Benchmark", "Engine did not start")));
    });
}

BenchmarkRunner::~BenchmarkRunner()
{
    stopEngines();
}

int BenchmarkRunner::gameCount(const BenchmarkSettings &s)
{
    const int n = int(s.engines.size());
    return n * (n - 1) / 2 * qMax(1, s.gamesPerPairing);
}

qint64 BenchmarkRunner::estimatedMs(const BenchmarkSettings &s)
{
    // A game rarely uses all of the time: about 70 % of both clocks plus 60 increments.
    const qint64 perGame = qint64(2 * (s.baseMs * 0.7 + 60 * s.incrementMs)) + 3000;
    return perGame * gameCount(s);
}

qint64 BenchmarkRunner::remainingMs() const
{
    if (!m_running)
        return -1;
    const int total = int(m_schedule.size());
    const int played = int(m_games.size());
    qint64 perGame = gameCount(m_settings) > 0 ? estimatedMs(m_settings) / gameCount(m_settings) : 0;
    if (played > 0) {
        qint64 sum = 0;
        for (const BenchmarkGame &g : m_games)
            sum += g.durationMs + 1500; // plus starting the engines
        const qint64 measured = sum / played;
        // trust the measured games more the more there are
        const double w = qMin(1.0, played / 4.0);
        perGame = qint64(w * measured + (1 - w) * perGame);
    }
    const qint64 current = m_inGame ? qMin(m_gameTimer.elapsed(), perGame) : 0;
    return qMax<qint64>(0, perGame * (total - played) - current);
}

void BenchmarkRunner::start(const BenchmarkSettings &settings)
{
    if (m_running)
        return;
    m_settings = settings;
    m_settings.gamesPerPairing = qMax(1, m_settings.gamesPerPairing);
    m_schedule.clear();
    m_games.clear();

    // Round robin by the circle method, so that every engine plays now and then
    // (instead of engine 1 playing all its games first).
    const int n = int(m_settings.engines.size());
    QVector<int> ring;
    for (int i = 0; i < n; ++i)
        ring << i;
    if (n % 2)
        ring << -1; // bye
    const int m = int(ring.size());
    QVector<QPair<int, int>> pairs;
    for (int round = 0; round < m - 1; ++round) {
        for (int i = 0; i < m / 2; ++i) {
            int a = ring[i], b = ring[m - 1 - i];
            if (a < 0 || b < 0)
                continue;
            if ((round + i) % 2)
                std::swap(a, b);
            pairs << qMakePair(a, b);
        }
        ring.insert(1, ring.takeLast());
    }
    int opening = 0;
    for (int g = 0; g < m_settings.gamesPerPairing; g += 2) {
        for (const auto &p : pairs) {
            const int o = opening++ % OpeningCount;
            m_schedule.push_back({ p.first, p.second, o });
            if (g + 1 < m_settings.gamesPerPairing)
                m_schedule.push_back({ p.second, p.first, o }); // same opening, colours swapped
        }
    }

    m_running = true;
    m_index = -1;
    m_total.start();
    QTimer::singleShot(0, this, &BenchmarkRunner::startGame);
}

void BenchmarkRunner::cancel()
{
    if (!m_running)
        return;
    m_running = false;
    m_inGame = false;
    m_flagTimer->stop();
    m_startTimer->stop();
    stopEngines();
    emit finished(true);
}

void BenchmarkRunner::stopEngines()
{
    for (Player &p : m_players) {
        if (p.engine) {
            p.engine->disconnect(this);
            p.engine->quit();
            p.engine->deleteLater();
        }
        p.engine = nullptr;
        p.ready = false;
    }
}

void BenchmarkRunner::startGame()
{
    if (!m_running)
        return;
    ++m_index;
    if (m_index >= m_schedule.size()) {
        m_running = false;
        emit finished(false);
        return;
    }
    const Pairing &pairing = m_schedule.at(m_index);
    m_current = BenchmarkGame();
    m_current.white = pairing.white;
    m_current.black = pairing.black;
    m_current.opening = trB(Openings[pairing.opening].name);

    Game &game = m_current.game;
    game.reset();
    m_openingPlies = 0;
    const QStringList moves = QString::fromLatin1(Openings[pairing.opening].moves).split(QLatin1Char(' '));
    for (const QString &u : moves) {
        const Move mv = parseUci(game.position(), u);
        if (mv.isNull() || !game.addMove(mv))
            break;
        ++m_openingPlies;
    }

    const EngineEntry &we = m_settings.engines.at(pairing.white);
    const EngineEntry &be = m_settings.engines.at(pairing.black);
    game.setTag(QStringLiteral("Event"), QStringLiteral("mbChess Benchmark"));
    game.setTag(QStringLiteral("Site"), QStringLiteral("mbChess"));
    game.setTag(QStringLiteral("Date"), QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd")));
    game.setTag(QStringLiteral("Round"), QString::number(m_index + 1));
    game.setTag(QStringLiteral("White"), we.name);
    game.setTag(QStringLiteral("Black"), be.name);
    game.setTag(QStringLiteral("TimeControl"),
                QStringLiteral("%1+%2").arg(m_settings.baseMs / 1000).arg(m_settings.incrementMs / 1000.0));
    game.setTag(QStringLiteral("Opening"), m_current.opening);

    m_remaining[0] = m_remaining[1] = m_settings.baseMs;
    m_inGame = true;
    m_gameTimer.start();

    for (int side = 0; side < 2; ++side) {
        const EngineEntry &entry = side == 0 ? we : be;
        auto *e = new UciEngine(this);
        m_players[side].engine = e;
        m_players[side].ready = false;
        m_players[side].name = entry.name;
        connect(e, &UciEngine::optionsKnown, this, [this, e, entry] { applyOptions(e, entry); });
        connect(e, &UciEngine::ready, this, [this, side] { onReady(side); });
        connect(e, &UciEngine::bestMove, this, [this, side](const QString &move, const QString &) {
            onBestMove(side, move);
        });
        connect(e, &UciEngine::errorOccurred, this, [this, side](const QString &msg) { onEngineError(side, msg); });
        connect(e, &UciEngine::infoReceived, this, [this, side](const engine::SearchInfo &info) {
            if (!m_inGame || side != thinkingSide() || info.multiPv > 1)
                return;
            if (info.hasScore) {
                const int sign = side == 0 ? 1 : -1; // UCI scores are from the engine's view
                m_lastEval.valid = true;
                m_lastEval.mate = info.isMate;
                m_lastEval.value = info.score * sign;
                m_lastEval.depth = info.depth;
            }
            emit searchInfo(side, info);
        });
        e->start(entry.path, entry.arguments, entry.protocol);
    }
    m_searching = false;
    m_startTimer->start(int(StartTimeoutMs));
    emit gameStarted(m_index, int(m_schedule.size()), we.name, be.name);
    emit positionChanged();
}

void BenchmarkRunner::applyOptions(UciEngine *e, const EngineEntry &entry)
{
    // The user's own options first (as in a normal game) ...
    for (auto it = entry.options.cbegin(); it != entry.options.cend(); ++it)
        if (e->hasOption(it.key()) && !enginemanager::isManagedOption(it.key()))
            e->setOption(it.key(), it.value());
    // ... then the same conditions for everybody.
    auto set = [e](const char *name, const QString &value) {
        if (e->hasOption(QLatin1String(name)))
            e->setOption(QLatin1String(name), value);
    };
    set("Threads", QString::number(m_settings.threads));
    set("Cores", QString::number(m_settings.threads));
    set("Hash", QString::number(m_settings.hashMb));
    set("OwnBook", QStringLiteral("false"));
    set("Ponder", QStringLiteral("false"));
    set("MultiPV", QStringLiteral("1"));
    set("UCI_AnalyseMode", QStringLiteral("false"));
    tablebases::applyToEngine(e, m_settings.tablebasePath, m_settings.gaviotaPath);

    const int elo = m_settings.elo;
    if (e->hasOption(QStringLiteral("UCI_LimitStrength")) && e->hasOption(QStringLiteral("UCI_Elo"))) {
        e->setOption(QStringLiteral("UCI_LimitStrength"), elo > 0 ? QStringLiteral("true") : QStringLiteral("false"));
        if (elo > 0) {
            const engine::EngineOption o = e->option(QStringLiteral("UCI_Elo"));
            const int lo = o.min.isEmpty() ? elo : o.min.toInt();
            const int hi = o.max.isEmpty() ? elo : o.max.toInt();
            e->setOption(QStringLiteral("UCI_Elo"), QString::number(qBound(lo, elo, hi)));
        }
    } else if (e->hasOption(QStringLiteral("Skill Level"))) {
        const engine::EngineOption o = e->option(QStringLiteral("Skill Level"));
        const int hi = o.max.isEmpty() ? 20 : o.max.toInt();
        const int skill = elo > 0 ? qBound(0, (elo - 1300) * hi / 1300, hi) : hi;
        e->setOption(QStringLiteral("Skill Level"), QString::number(skill));
    } else if (e->hasOption(QStringLiteral("Skill_Level"))) { // Patricia
        e->setOption(QStringLiteral("Skill_Level"), QString::number(enginemanager::patriciaSkillLevel(elo)));
    }
}

void BenchmarkRunner::onReady(int side)
{
    if (!m_inGame || m_players[side].ready)
        return;
    m_players[side].ready = true;
    m_players[side].engine->newGame();
    if (m_players[0].ready && m_players[1].ready) {
        m_startTimer->stop();
        m_gameTimer.restart(); // the start of the engines does not count
        requestMove();
    }
}

void BenchmarkRunner::requestMove()
{
    if (!m_inGame)
        return;
    const Game &game = m_current.game;
    const int side = game.position().sideToMove() == White ? 0 : 1;
    GoParameters p;
    p.whiteTime = qMax<qint64>(1, m_remaining[0]);
    p.blackTime = qMax<qint64>(1, m_remaining[1]);
    p.whiteIncrement = p.blackIncrement = m_settings.incrementMs;
    m_moveTimer.start();
    m_searching = true;
    m_lastEval = Evaluation();
    m_flagTimer->start(int(qMin<qint64>(m_remaining[side] + FlagGraceMs, 24LL * 3600 * 1000)));
    m_players[side].engine->go(QString(), game.uciMoves(), p);
    emit positionChanged();
}

void BenchmarkRunner::onBestMove(int side, const QString &text)
{
    if (!m_inGame)
        return;
    Game &game = m_current.game;
    const int toMove = game.position().sideToMove() == White ? 0 : 1;
    if (side != toMove)
        return;
    m_flagTimer->stop();
    m_searching = false;
    const qint64 used = m_moveTimer.elapsed();
    m_current.thinkMs[side] += used;
    m_remaining[side] -= used;
    const Result loss = side == 0 ? Result::BlackWins : Result::WhiteWins;
    if (m_remaining[side] < 0) {
        endGame(loss, trB(QT_TRANSLATE_NOOP("Benchmark", "Time forfeit")));
        return;
    }
    m_remaining[side] += m_settings.incrementMs;

    const Move m = parseUci(game.position(), text);
    if (m.isNull() || !game.addMove(m)) {
        endGame(loss, trB(QT_TRANSLATE_NOOP("Benchmark", "Illegal move")) + QStringLiteral(" (%1)").arg(text));
        return;
    }
    ++m_current.moves[side];
    if (m_lastEval.valid)
        game.setEvaluation(game.currentNode(), m_lastEval);

    switch (game.status()) {
    case GameStatus::Checkmate:
        endGame(side == 0 ? Result::WhiteWins : Result::BlackWins, trB(QT_TRANSLATE_NOOP("Benchmark", "Checkmate")));
        return;
    case GameStatus::Stalemate:
        endGame(Result::Draw, trB(QT_TRANSLATE_NOOP("Benchmark", "Stalemate")));
        return;
    case GameStatus::InsufficientMaterial:
        endGame(Result::Draw, trB(QT_TRANSLATE_NOOP("Benchmark", "Insufficient material")));
        return;
    case GameStatus::FiftyMoveRule:
        endGame(Result::Draw, trB(QT_TRANSLATE_NOOP("Benchmark", "Fifty-move rule")));
        return;
    case GameStatus::ThreefoldRepetition:
        endGame(Result::Draw, trB(QT_TRANSLATE_NOOP("Benchmark", "Threefold repetition")));
        return;
    default:
        break;
    }
    if (game.plyCount() >= MaxPlies) {
        endGame(Result::Draw, trB(QT_TRANSLATE_NOOP("Benchmark", "Move limit (adjudicated)")));
        return;
    }
    requestMove();
}

void BenchmarkRunner::onEngineError(int side, const QString &message)
{
    if (!m_inGame)
        return;
    endGame(side == 0 ? Result::BlackWins : Result::WhiteWins,
            trB(QT_TRANSLATE_NOOP("Benchmark", "Engine error")) + QStringLiteral(": ") + message);
}

void BenchmarkRunner::onTimeout()
{
    if (!m_inGame)
        return;
    const int side = m_current.game.position().sideToMove() == White ? 0 : 1;
    m_current.thinkMs[side] += m_moveTimer.elapsed();
    endGame(side == 0 ? Result::BlackWins : Result::WhiteWins, trB(QT_TRANSLATE_NOOP("Benchmark", "Time forfeit")));
}

void BenchmarkRunner::endGame(Result result, const QString &reason)
{
    if (!m_inGame)
        return;
    m_inGame = false;
    m_searching = false;
    m_flagTimer->stop();
    m_startTimer->stop();
    m_current.result = result;
    m_current.reason = reason;
    m_current.plies = m_current.game.plyCount();
    m_current.durationMs = m_gameTimer.elapsed();
    m_current.game.setResult(result);
    m_current.game.setTag(QStringLiteral("Result"), resultToString(result));
    m_current.game.setTag(QStringLiteral("Termination"), reason);
    stopEngines();
    m_games.push_back(m_current);
    emit gameFinished(m_current);
    QTimer::singleShot(m_pauseMs, this, &BenchmarkRunner::startGame);
}

qint64 BenchmarkRunner::clockMs(int side) const
{
    if (side < 0 || side > 1)
        return 0;
    qint64 ms = m_remaining[side];
    if (m_inGame && m_searching && thinkingSide() == side)
        ms -= m_moveTimer.elapsed();
    return qMax<qint64>(0, ms);
}

int BenchmarkRunner::thinkingSide() const
{
    if (!m_inGame || !m_searching)
        return -1;
    return m_current.game.position().sideToMove() == White ? 0 : 1;
}
