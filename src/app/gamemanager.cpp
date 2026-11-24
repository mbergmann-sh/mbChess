// SPDX-License-Identifier: GPL-3.0-or-later
#include "gamemanager.h"

#include "chessclock.h"
#include "datafolders.h"
#include "enginemanager.h"
#include "languagemanager.h"
#include "opponents.h"
#include "tablebases.h"

#include <book/bookbuilder.h>
#include <chesscore/notation.h>
#include <pgn/pgn.h>

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDate>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QPointer>
#include <QRandomGenerator>
#include <QSaveFile>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>

using namespace mbc;
using mbc::engine::SearchInfo;
using mbc::engine::UciEngine;

GameManager::GameManager(LanguageManager *languages, QObject *parent)
    : QObject(parent)
    , m_languages(languages)
    , m_engine(new UciEngine(this))
    , m_engine2(new UciEngine(this))
    , m_clock(new ChessClock(this))
{
    connectEngine(m_engine);
    connectEngine(m_engine2);
    connect(m_clock, &ChessClock::flagFell, this, &GameManager::onFlagFell);
}

GameManager::~GameManager()
{
    if (m_chessbookCancel)
        *m_chessbookCancel = true;
    m_engine->quit();
    m_engine2->quit();
}

// ---------------------------------------------------------------------------
// State

UciEngine *GameManager::engineFor(Color c) const
{
    return m_settings.engineVsEngine() && c == Black ? m_engine2 : m_engine;
}

QString GameManager::engineDisplayName(const UciEngine *engine) const
{
    const QString &given = engine == m_engine2 ? m_engine2DisplayName : m_engineDisplayName;
    if (!given.isEmpty())
        return given;
    return engine->name().isEmpty() ? enginemanager::entry(engine->program()).name : engine->name();
}

bool GameManager::anyEngineThinking() const
{
    // pondering is thinking on the human's time, not a search for a move
    if (m_builtinThinking && m_searchNode >= 0)
        return true;
    for (const UciEngine *e : { m_engine, m_engine2 })
        if (e->state() == UciEngine::State::Thinking && !e->isPondering())
            return true;
    return false;
}

bool GameManager::isPondering() const
{
    return m_engine->isPondering() || m_engine2->isPondering();
}

bool GameManager::anyEngineStarting() const
{
    return m_engine->state() == UciEngine::State::Starting
        || (m_settings.engineVsEngine() && m_engine2->state() == UciEngine::State::Starting);
}

void GameManager::setGameOver(bool over, const QString &reason)
{
    m_gameOver = over;
    m_endReason = reason;
}

bool GameManager::isPlayingMode() const
{
    return m_settings.hasEngine() || m_clock->timeControl().enabled();
}

bool GameManager::isEngineTurn() const
{
    if (m_suspended)
        return false;
    // The engine moves at the end of the game, wherever the user is looking.
    return m_settings.hasEngine() && !m_gameOver
        && m_settings.isEngineSide(m_game.nodePosition(mainLineEnd()).sideToMove());
}

bool GameManager::humanMayMove() const
{
    if (m_waitingForGame || m_suspended)
        return false;
    if (isPlayingMode()) {
        if (m_gameOver || !m_game.isAtLineEnd())
            return false;
        return !m_settings.isEngineSide(m_game.position().sideToMove());
    }
    // Analysis: anywhere, except at the end of a finished game.
    return !(m_gameOver && m_game.currentNode() == mainLineEnd());
}

bool GameManager::gameInProgress() const
{
    return !m_suspended && !m_waitingForGame && !m_gameOver && isPlayingMode() && m_game.plyCount() > 0;
}

Game::NodeId GameManager::mainLineEnd() const
{
    const QVector<Game::NodeId> line = m_game.mainLine();
    return line.isEmpty() ? Game::RootNode : line.last();
}

QString GameManager::statusReason(GameStatus status)
{
    switch (status) {
    case GameStatus::Checkmate: return tr("Checkmate.");
    case GameStatus::Stalemate: return tr("Stalemate.");
    case GameStatus::InsufficientMaterial: return tr("Draw: insufficient material.");
    case GameStatus::FiftyMoveRule: return tr("Draw: fifty-move rule.");
    case GameStatus::ThreefoldRepetition: return tr("Draw: threefold repetition.");
    case GameStatus::Ongoing: break;
    }
    return QString();
}

void GameManager::markSaved()
{
    m_savedPgn = pgn::write(m_game);
}

bool GameManager::hasUnsavedChanges() const
{
    // moves, comments or variations that are not in a file yet (benchmark games: results window)
    if (m_suspended || (m_game.plyCount() == 0 && m_game.comment(Game::RootNode).isEmpty()))
        return false;
    return pgn::write(m_game) != m_savedPgn;
}

// ---------------------------------------------------------------------------
// Starting and ending

void GameManager::startGame(const GameSettings &settings, const Position &start)
{
    m_waitingForGame = false;
    m_positionSetUp = false;
    m_engine->stop(true);
    m_engine2->stop(true);
    m_settings = settings;
    m_clock->setTimeControl(settings.timeControl);

    m_game.reset(start);
    m_game.setTag(QStringLiteral("Event"), QStringLiteral("mbChess"));
    m_game.setTag(QStringLiteral("Date"), QDate::currentDate().toString(QStringLiteral("yyyy.MM.dd")));
    m_game.setTag(QStringLiteral("Round"), QStringLiteral("-"));
    if (settings.timeControl.enabled())
        m_game.setTag(QStringLiteral("TimeControl"), settings.timeControl.pgnTag());
    m_gameOver = false;
    m_endReason.clear();
    m_engineLine.clear();
    m_searchNode = -1;
    m_ponderNode = -1;
    m_lastEval = Evaluation();
    loadBook();
    loadChessbook();

    m_builtinThinking = false;
    if (settings.hasEngine() && settings.builtinPlayer != GameSettings::NoBuiltin) {
        // played by mbChess itself (menu Play > Beginners): no engine
        m_engine->quit();
        m_engine2->quit();
        updatePlayerNames();
        emit changed();
        if (isEngineTurn())
            startEngineSearch();
        return;
    }
    if (settings.hasEngine()) {
        prepareEngine(m_engine, settings.enginePath, &m_engineBookSetup, &m_engineDisplayName);
        if (settings.engineVsEngine())
            prepareEngine(m_engine2, settings.engine2Path, &m_engine2BookSetup, &m_engine2DisplayName);
        else
            m_engine2->quit();
    } else {
        m_engine->quit();
        m_engine2->quit();
    }
    updatePlayerNames();
    emit changed();
    if (settings.hasEngine() && !settings.engineVsEngine() && settings.colorChoice == GameSettings::PlayRandom)
        emit statusMessage(settings.engineColor() == Black ? tr("Drawn by lot: you play White.")
                                                           : tr("Drawn by lot: you play Black."),
                           8000);
}

void GameManager::loadGame(const Game &game)
{
    GameSettings hvh = m_settings;
    hvh.mode = GameSettings::HumanVsHuman;
    hvh.builtinPlayer = GameSettings::NoBuiltin;
    hvh.opponentName.clear();
    hvh.engineOptions.clear();
    hvh.timeControl = TimeControl();
    startGame(hvh, game.startPosition());
    m_game = game;
    markSaved(); // unchanged since it was read
    m_positionSetUp = false;
    m_gameOver = m_game.result() != Result::Unknown;
    m_endReason = m_gameOver ? tr("Game loaded.") : QString();
    updatePlayerNames();
    emit changed();
}

void GameManager::usePosition(const Position &pos)
{
    GameSettings hvh = m_settings;
    hvh.mode = GameSettings::HumanVsHuman;
    hvh.builtinPlayer = GameSettings::NoBuiltin;
    hvh.opponentName.clear();
    hvh.engineOptions.clear();
    hvh.timeControl = TimeControl();
    startGame(hvh, pos);
    m_positionSetUp = true;
}

void GameManager::stopPlaying()
{
    m_engine->quit();
    m_engine2->quit();
    m_clock->stop();
    m_searchNode = -1;
    m_settings.mode = GameSettings::HumanVsHuman; // afterwards the game can be analysed
    m_settings.timeControl = TimeControl();
    m_waitingForGame = false;
}

void GameManager::endGame(Result result, const QString &reason)
{
    m_gameOver = true;
    m_endReason = reason;
    m_game.setResult(result);
    m_clock->stop();
    m_engine->stop(true);
    m_engine2->stop(true);
    m_searchNode = -1;
    emit changed();
    emit gameEnded(result, reason);
}

void GameManager::updatePlayerNames()
{
    const QString player = m_settings.playerName.isEmpty() ? tr("Player") : m_settings.playerName;
    // An opponent of the menu Play: its name on the clock, with the engine in the PGN
    QString opponent = engineDisplayName(m_engine);
    QString opponentTag = opponent;
    if (!m_settings.opponentName.isEmpty()) {
        opponent = m_settings.opponentName;
        opponentTag = m_settings.builtinPlayer != GameSettings::NoBuiltin
            ? QStringLiteral("%1 (mbChess)").arg(opponent)
            : QStringLiteral("%1 (%2)").arg(opponent, engineDisplayName(m_engine));
    }
    QString white, black;
    switch (m_settings.mode) {
    case GameSettings::HumanVsHuman:
        white = m_game.tag(QStringLiteral("White"));
        black = m_game.tag(QStringLiteral("Black"));
        if (white.isEmpty() || white == QLatin1String("?"))
            white = tr("White");
        if (black.isEmpty() || black == QLatin1String("?"))
            black = tr("Black");
        break;
    case GameSettings::HumanWhiteVsEngine:
        white = player;
        black = opponent;
        break;
    case GameSettings::HumanBlackVsEngine:
        white = opponent;
        black = player;
        break;
    case GameSettings::EngineVsEngine:
        white = engineDisplayName(m_engine);
        black = engineDisplayName(m_engine2);
        break;
    }
    if (m_settings.hasEngine()) {
        m_game.setTag(QStringLiteral("White"), m_settings.mode == GameSettings::HumanBlackVsEngine ? opponentTag : white);
        m_game.setTag(QStringLiteral("Black"), m_settings.mode == GameSettings::HumanWhiteVsEngine ? opponentTag : black);
    }
    emit playerNamesChanged(white, black);
}

void GameManager::checkGameEnd()
{
    // In an analysis, the end of a variation does not end the game.
    if (!isPlayingMode() && !m_game.isMainLine(m_game.currentNode()))
        return;
    const GameStatus status = m_game.status();
    switch (status) {
    case GameStatus::Checkmate:
        endGame(m_game.position().sideToMove() == White ? Result::BlackWins : Result::WhiteWins, statusReason(status));
        break;
    case GameStatus::Stalemate:
    case GameStatus::InsufficientMaterial:
    case GameStatus::FiftyMoveRule:
    case GameStatus::ThreefoldRepetition:
        endGame(Result::Draw, statusReason(status));
        break;
    case GameStatus::Ongoing:
        break;
    }
}

void GameManager::onFlagFell(Color side)
{
    if (m_gameOver || m_suspended) // the benchmark decides time forfeits itself
        return;
    endGame(side == White ? Result::BlackWins : Result::WhiteWins,
            tr("%1 lost on time.").arg(side == White ? tr("White") : tr("Black")));
}

// ---------------------------------------------------------------------------
// Moves

void GameManager::applyMove(Move move)
{
    if (!m_game.addMove(move))
        return;
    afterMove();
}

void GameManager::afterMove()
{
    emit movePlayed(m_game.nodeSan(m_game.currentNode()));
    if (m_clock->timeControl().enabled()) {
        if (!m_clock->isRunning())
            m_clock->start(m_game.position().sideToMove()); // clock starts after the first move
        else
            m_clock->switchSide();
    }
    checkGameEnd();
    emit changed();
    if (isEngineTurn())
        startEngineSearch();
}

void GameManager::takeBack()
{
    if (m_game.plyCount() == 0 || !m_game.isAtLineEnd())
        return;
    if (!isPlayingMode() && m_game.currentNode() != mainLineEnd()) {
        m_game.takeBack(); // the end of a variation
        emit changed();
        return;
    }
    if (m_settings.engineVsEngine())
        return; // nothing to take back for a human
    m_engine->stop(true);
    m_searchNode = -1;
    m_ponderNode = -1;
    m_gameOver = false;
    m_endReason.clear();
    m_game.setResult(Result::Unknown);
    m_game.takeBack();
    // Against an engine: go back until it is the human's turn again.
    if (m_settings.hasEngine())
        while (m_game.plyCount() > 0 && m_game.position().sideToMove() == m_settings.engineColor())
            m_game.takeBack();
    if (m_clock->timeControl().enabled() && m_clock->isRunning())
        m_clock->start(m_game.position().sideToMove());
    m_engineLine.clear();
    emit changed();
    if (isEngineTurn()) // engine plays White and everything was taken back
        startEngineSearch();
}

void GameManager::moveNow()
{
    for (UciEngine *e : { m_engine, m_engine2 })
        if (e->state() == UciEngine::State::Thinking && !e->isPondering())
            e->stop(false);
}

void GameManager::resign()
{
    if (m_gameOver || m_game.currentNode() != mainLineEnd())
        return;
    Color loser = m_game.position().sideToMove();
    if (m_settings.hasEngine() && !m_settings.engineVsEngine())
        loser = ~m_settings.engineColor();
    endGame(loser == White ? Result::BlackWins : Result::WhiteWins,
            tr("%1 resigns.").arg(loser == White ? tr("White") : tr("Black")));
}

void GameManager::promoteVariation(Game::NodeId id)
{
    if (isPlayingMode())
        return;
    const Game::NodeId oldEnd = mainLineEnd();
    m_game.promoteVariation(id);
    afterTreeChange(oldEnd);
}

void GameManager::deleteVariation(Game::NodeId id)
{
    if (isPlayingMode())
        return;
    const Game::NodeId oldEnd = mainLineEnd();
    m_game.deleteFrom(id);
    afterTreeChange(oldEnd);
}

void GameManager::afterTreeChange(Game::NodeId oldMainEnd)
{
    // The result belongs to the end of the main line; a different end has none yet.
    if (mainLineEnd() != oldMainEnd && (m_gameOver || m_game.result() != Result::Unknown)) {
        m_gameOver = false;
        m_endReason.clear();
        m_game.setResult(Result::Unknown);
    }
    emit changed();
}

// ---------------------------------------------------------------------------
// Engines

void GameManager::connectEngine(UciEngine *e)
{
    connect(e, &UciEngine::ready, this, [this, e] { onEngineReady(e); });
    connect(e, &UciEngine::optionsKnown, this, [this, e] {
        applyUserEngineOptions(e);
        applyEngineBookOptions(e);
        tablebases::applyToEngine(e, tablebases::activePath());
    });
    connect(e, &UciEngine::bestMove, this,
            [this, e](const QString &move, const QString &ponder) { onEngineBestMove(e, move, ponder); });
    connect(e, &UciEngine::infoReceived, this, [this, e](const SearchInfo &info) { onEngineInfo(e, info); });
    connect(e, &UciEngine::errorOccurred, this, &GameManager::engineError);
    connect(e, &UciEngine::stateChanged, this, &GameManager::changed);
}

void GameManager::prepareEngine(UciEngine *e, const QString &path, QString *bookSetupState, QString *displayName)
{
    // Engines read their book and many options only at start: restart on changes.
    QString bookSetup = m_settings.engineOwnBook ? QStringLiteral("on:") + m_settings.engineBookFile : QStringLiteral("off");
    const EngineEntry entry = enginemanager::entry(path);
    for (auto it = entry.options.cbegin(); it != entry.options.cend(); ++it)
        bookSetup += QLatin1Char('|') + it.key() + QLatin1Char('=') + it.value();
    for (auto it = m_settings.engineOptions.cbegin(); it != m_settings.engineOptions.cend(); ++it)
        bookSetup += QStringLiteral("|play:") + it.key() + QLatin1Char('=') + it.value(); // restart for other options
    bookSetup += QStringLiteral("|tb=") + tablebases::activePath() + QStringLiteral("|gtb=") + tablebases::gaviotaActivePath();
    *displayName = entry.name != entry.defaultName ? entry.name : QString();
    const bool sameEngine = e->program() == path && e->state() != UciEngine::State::NotRunning
        && *bookSetupState == bookSetup; // engines read their book only at start
    *bookSetupState = bookSetup;
    if (!sameEngine)
        e->start(path, entry.arguments, entry.protocol); // continues in onEngineReady()
    else if (e->state() != UciEngine::State::Starting)
        onEngineReady(e);
}

void GameManager::applyUserEngineOptions(UciEngine *engine)
{
    // Options from "Manage engines", before the engine initialises (first "isready")
    const EngineEntry entry = enginemanager::entry(engine->program());
    for (auto it = entry.options.cbegin(); it != entry.options.cend(); ++it)
        if (engine->hasOption(it.key()) && !enginemanager::isManagedOption(it.key()))
            engine->setOption(it.key(), it.value());
}

void GameManager::applyEngineBookOptions(UciEngine *engine)
{
    // Fruit, Toga, GNU Chess: OwnBook + BookFile, Glaurung: OwnBook + "Book File" (PolyGlot); Arasan: OwnBook + BookPath
    // (its own format). Engines without such options (Stockfish, Viridithas) have no book.
    if (!engine->hasOption(QStringLiteral("OwnBook")))
        return;
    engine->setOption(QStringLiteral("OwnBook"), m_settings.engineOwnBook ? QStringLiteral("true") : QStringLiteral("false"));
    if (!m_settings.engineOwnBook)
        return;
    const QString own = QDir::toNativeSeparators(m_settings.engineBookFile);
    // "BookFile" (Fruit, Toga, GNU Chess) or "Book File" (Glaurung), both PolyGlot
    const QString bookOption = engine->hasOption(QStringLiteral("BookFile")) ? QStringLiteral("BookFile")
        : engine->hasOption(QStringLiteral("Book File"))                    ? QStringLiteral("Book File")
                                                                               : QString();
    if (!bookOption.isEmpty()) {
        // the engine's default book if it is really there, otherwise mbChess' book
        const QString def = engine->option(bookOption).defaultValue;
        const bool hasDefault = !def.isEmpty() && QFileInfo(QFileInfo(engine->program()).dir(), def).isFile();
        if (own.isEmpty() && hasDefault)
            return;
        const QString file = own.isEmpty() ? QDir::toNativeSeparators(builtInBookFile()) : own;
        if (!file.isEmpty())
            engine->setOption(bookOption, file);
    } else if (engine->hasOption(QStringLiteral("BookPath")) && !own.isEmpty()) {
        engine->setOption(QStringLiteral("BookPath"), own);
    }
}

void GameManager::onEngineReady(UciEngine *engine)
{
    if (!m_settings.hasEngine() || (engine == m_engine2 && !m_settings.engineVsEngine()))
        return;
    engine->setOption(QStringLiteral("Threads"), QString::number(m_settings.threads));
    engine->setOption(QStringLiteral("Hash"), QString::number(m_settings.hashMb));

    const int elo = m_settings.strengthElo;
    if (engine->hasOption(QStringLiteral("UCI_LimitStrength")) && engine->hasOption(QStringLiteral("UCI_Elo"))) {
        engine->setOption(QStringLiteral("UCI_LimitStrength"), elo > 0 ? QStringLiteral("true") : QStringLiteral("false"));
        if (elo > 0) {
            const engine::EngineOption o = engine->option(QStringLiteral("UCI_Elo"));
            const int lo = o.min.isEmpty() ? elo : o.min.toInt();
            const int hi = o.max.isEmpty() ? elo : o.max.toInt();
            engine->setOption(QStringLiteral("UCI_Elo"), QString::number(qBound(lo, elo, hi)));
        }
    } else if (engine->hasOption(QStringLiteral("Skill Level"))) {
        const int skill = elo > 0 ? qBound(0, (elo - 1300) * 20 / 1300, 20) : 20;
        engine->setOption(QStringLiteral("Skill Level"), QString::number(skill));
    } else if (engine->hasOption(QStringLiteral("Skill_Level"))) { // Patricia
        engine->setOption(QStringLiteral("Skill_Level"), QString::number(enginemanager::patriciaSkillLevel(elo)));
    }
    // extra options of an opponent of the menu Play (e.g. Texel "Materialistic")
    for (auto it = m_settings.engineOptions.cbegin(); it != m_settings.engineOptions.cend(); ++it)
        if (engine->hasOption(it.key()))
            engine->setOption(it.key(), it.value());
    // UCI: the GUI tells the engine whether it will be asked to ponder
    if (engine->hasOption(QStringLiteral("Ponder")))
        engine->setOption(QStringLiteral("Ponder"), ponderAllowed(engine) ? QStringLiteral("true") : QStringLiteral("false"));
    engine->newGame();
    updatePlayerNames();
    emit changed();
    if (isEngineTurn() && engineFor(m_game.nodePosition(mainLineEnd()).sideToMove()) == engine)
        startEngineSearch();
}

engine::GoParameters GameManager::goParameters(Color side) const
{
    engine::GoParameters p;
    if (m_clock->timeControl().enabled() && m_settings.strengthElo > 0) {
        // Weaker levels should answer quickly, as a human of that strength would,
        // instead of spending the full time budget the engine would choose.
        const TimeControl &tc = m_clock->timeControl();
        const qint64 own = m_clock->remaining(side);
        p.moveTime = qBound(qint64(200), own / 40 + tc.incrementMs * 3 / 4, qint64(2500));
    } else if (m_clock->timeControl().enabled()) {
        p.whiteTime = m_clock->remaining(White);
        p.blackTime = m_clock->remaining(Black);
        p.whiteIncrement = p.blackIncrement = m_clock->timeControl().incrementMs;
    } else {
        p.moveTime = m_settings.strengthElo > 0 ? 1500 : 3000;
    }
    return p;
}

QString GameManager::startFenForEngine() const
{
    const QString fen = m_game.startPosition().toFen();
    return fen == QLatin1String(Position::StartFen) ? QString() : fen;
}

void GameManager::startEngineSearch()
{
    const Color side = m_game.nodePosition(mainLineEnd()).sideToMove();
    if (m_settings.hasEngine() && m_settings.builtinPlayer != GameSettings::NoBuiltin) {
        startBuiltinMove();
        return;
    }
    UciEngine *const engine = engineFor(side);
    if (engine->state() != UciEngine::State::Ready && engine->state() != UciEngine::State::Thinking)
        return; // not loaded yet: onEngineReady() starts the search
    if (m_game.currentNode() != mainLineEnd())
        m_game.goToNode(mainLineEnd());
    if (engine->isPondering()) {
        // Did the human play the expected move? Then the ponder search goes on as the
        // real one ("ponderhit"), with the time it has already used.
        const Game::NodeId cur = m_game.currentNode();
        const bool hit = m_ponderNode >= 0 && cur != Game::RootNode && m_game.parent(cur) == m_ponderNode
            && toUci(m_game.nodeMove(cur)) == m_ponderMove;
        m_ponderNode = -1;
        if (hit) {
            m_engineLine.clear();
            m_searchNode = cur;
            m_lastEval = Evaluation();
            m_lastPv.clear();
            engine->ponderHit();
            emit changed();
            return;
        }
        engine->stop(true); // another move: search the real position
    }
    if (playBookMove())
        return;
    const engine::GoParameters p = goParameters(side);
    m_engineLine.clear();
    m_searchNode = m_game.currentNode();
    m_lastEval = Evaluation();
    m_lastPv.clear();
    engine->go(startFenForEngine(), m_game.uciMoves(), p);
    emit changed();
}

void GameManager::startBuiltinMove()
{
    // mbChess plays itself (menu Play > Beginners), after a short pause like a human
    if (m_game.currentNode() != mainLineEnd())
        m_game.goToNode(mainLineEnd());
    m_engineLine.clear();
    m_searchNode = m_game.currentNode();
    m_builtinThinking = true;
    const Game::NodeId from = m_searchNode;
    const quint64 hash = m_game.position().hash();
    const int delay = 500 + int(QRandomGenerator::global()->bounded(1000));
    QTimer::singleShot(delay, this, [this, from, hash] {
        // only if nothing happened in between (new game, take back, ...)
        if (m_searchNode != from || !m_game.isValidNode(from) || m_game.nodePosition(from).hash() != hash
            || m_gameOver || m_settings.builtinPlayer == GameSettings::NoBuiltin)
            return;
        m_builtinThinking = false;
        m_searchNode = -1;
        if (!m_game.children(from).isEmpty())
            return;
        const Move m = opponents::builtinMove(m_game.nodePosition(from), m_settings.builtinPlayer);
        if (m.isNull())
            return;
        if (m_game.currentNode() != from)
            m_game.goToNode(from);
        if (m_game.addMove(m))
            afterMove();
    });
    emit changed();
}

bool GameManager::ponderAllowed(const UciEngine *engine) const
{
    // Only against a human and at full strength: in "engine against engine" both would
    // share the processor, and a weakened engine should not get extra time.
    return m_settings.ponder && m_settings.hasEngine() && !m_settings.engineVsEngine() && m_settings.strengthElo == 0
        && !m_suspended && engine->protocol() == engine::Protocol::Uci && engine->hasOption(QStringLiteral("Ponder"));
}

void GameManager::startPondering(UciEngine *engine, const QString &ponderMove)
{
    m_ponderNode = -1;
    if (m_gameOver || isEngineTurn() || !ponderAllowed(engine) || m_game.currentNode() != mainLineEnd())
        return;
    const QString expected = ponderMove.isEmpty() ? m_lastPv.value(1) : ponderMove;
    if (expected.isEmpty() || parseUci(m_game.position(), expected).isNull())
        return;
    engine::GoParameters p = goParameters(~m_game.position().sideToMove());
    p.ponder = true;
    m_ponderNode = m_game.currentNode();
    m_ponderMove = expected;
    engine->go(startFenForEngine(), m_game.uciMoves() << expected, p);
}

void GameManager::onEngineBestMove(UciEngine *engine, const QString &move, const QString &ponder)
{
    const Game::NodeId from = m_searchNode;
    if (from >= 0 && m_game.isValidNode(from) && engine != engineFor(m_game.nodePosition(from).sideToMove()))
        return; // the other engine's (cancelled) search
    m_searchNode = -1;
    if (!m_settings.hasEngine() || m_gameOver || from < 0 || !m_game.isValidNode(from)
        || !m_game.children(from).isEmpty() || !m_settings.isEngineSide(m_game.nodePosition(from).sideToMove()))
        return;
    const Move m = parseUci(m_game.nodePosition(from), move);
    if (m.isNull()) {
        emit engineError(tr("The engine sent an illegal move: %1").arg(move));
        return;
    }
    if (m_game.currentNode() != from)
        m_game.goToNode(from); // the user was looking at an earlier move
    if (!m_game.addMove(m))
        return;
    if (m_lastEval.valid)
        m_game.setEvaluation(m_game.currentNode(), m_lastEval);
    afterMove();
    startPondering(engine, ponder);
}

void GameManager::onEngineInfo(UciEngine *engine, const SearchInfo &info)
{
    // Lines without score or main line (currmove, hashfull, ...) would overwrite the display.
    if (m_searchNode < 0 || !m_settings.hasEngine() || info.multiPv != 1 || (!info.hasScore && info.pv.isEmpty()))
        return;
    const Color engineSide = m_game.nodePosition(m_searchNode).sideToMove();
    if (engine != engineFor(engineSide))
        return;
    m_engineLine = engineInfoText(m_game.nodePosition(m_searchNode), engineSide, info, &m_lastEval);
    if (!info.pv.isEmpty())
        m_lastPv = info.pv;
    emit engineOutputChanged(m_engineLine);
}

QString GameManager::engineInfoText(const Position &root, Color engineSide, const SearchInfo &info, Evaluation *eval) const
{
    QStringList parts;
    parts << tr("Depth %1").arg(info.depth);
    if (info.hasScore) {
        // UCI scores are from the engine's point of view; show them from White's.
        const int sign = engineSide == White ? 1 : -1;
        const int s = info.score * sign;
        eval->valid = true;
        eval->mate = info.isMate;
        eval->value = s;
        eval->depth = info.depth;
        parts << (info.isMate ? (s > 0 ? QStringLiteral("#%1").arg(s) : QStringLiteral("-#%1").arg(-s))
                              : QStringLiteral("%1%2").arg(s >= 0 ? QStringLiteral("+") : QString())
                                    .arg(s / 100.0, 0, 'f', 2));
    }
    if (info.nps > 0)
        parts << tr("%1 kN/s").arg(info.nps / 1000);
    // Main line in the user's notation
    QStringList san;
    Position pos = root;
    const QString letters = m_languages->pieceLetters();
    for (int i = 0; i < info.pv.size() && i < 10; ++i) {
        const Move m = parseUci(pos, info.pv.at(i));
        if (m.isNull())
            break;
        san << localizeSan(toSan(pos, m), letters);
        pos.makeMove(m);
    }
    QString text = parts.join(QStringLiteral("  ·  "));
    if (!san.isEmpty())
        text += QLatin1Char('\n') + san.join(QLatin1Char(' '));
    return text;
}

// ---------------------------------------------------------------------------
// Opening book and chessbook

QString GameManager::builtInBookFile()
{
    // Engines need a real file: the built-in book is copied out of the program once.
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/books");
    const QString path = dir + QStringLiteral("/mbchess.bin");
    QFile res(QStringLiteral(":/books/mbchess.bin"));
    if (QFileInfo(path).size() != res.size()) {
        QDir().mkpath(dir);
        QFile::remove(path);
        if (!res.copy(path))
            return QString();
        QFile::setPermissions(path, QFile::ReadOwner | QFile::WriteOwner | QFile::ReadGroup | QFile::ReadOther);
    }
    return path;
}

void GameManager::loadBook()
{
    m_book.close();
    if (!m_settings.hasEngine() || !m_settings.useBook)
        return;
    const QString file = m_settings.bookFile.isEmpty() ? QStringLiteral(":/books/mbchess.bin") : m_settings.bookFile;
    QString error;
    if (!m_book.open(file, &error))
        QTimer::singleShot(0, this, [this, file, error] {
            emit statusMessage(tr("Opening book %1 could not be read: %2").arg(QDir::toNativeSeparators(file), error),
                               10000);
        });
}

void GameManager::loadChessbook()
{
    // Game databases become a book with the results of the games (as in Crafty). The
    // book is built once in the background and kept, until a database changes.
    QStringList files;
    if (m_settings.hasEngine() && m_settings.useChessbook) {
        if (m_settings.chessbookFile == QLatin1String("*")) {
            const QFileInfoList list = QDir(datafolders::chessbooksFolder())
                                           .entryInfoList({ QStringLiteral("*.pgn") }, QDir::Files, QDir::Name);
            for (const QFileInfo &f : list)
                files << f.absoluteFilePath();
        } else if (QFileInfo(m_settings.chessbookFile).isFile()) {
            files << m_settings.chessbookFile;
        }
    }
    const int plies = 2 * m_settings.chessbookDepth;
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(QByteArrayLiteral("mbchess-chessbook-1"));
    hash.addData(QByteArray::number(plies));
    for (const QString &f : files) {
        const QFileInfo info(f);
        hash.addData(f.toUtf8() + '|' + QByteArray::number(info.size()) + '|'
                     + QByteArray::number(info.lastModified().toMSecsSinceEpoch()));
    }
    const QString cache = files.isEmpty() ? QString()
        : QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/chessbooks/")
            + QString::fromLatin1(hash.result().toHex()) + QStringLiteral(".bin");
    if (cache == m_chessbookKey && (m_chessbook.isOpen() || m_chessbookThread))
        return; // already there or being built
    m_chessbookKey = cache;
    m_chessbook.close();
    if (m_chessbookCancel)
        *m_chessbookCancel = true; // an older build is no longer needed
    emit chessbookProgress(QString());
    if (cache.isEmpty())
        return;
    if (m_chessbook.open(cache))
        return;

    auto cancel = std::make_shared<std::atomic<bool>>(false);
    m_chessbookCancel = cancel;
    QPointer<GameManager> self(this);
    emit chessbookProgress(tr("reading games..."));
    QThread *thread = QThread::create([self, files, plies, cache, cancel] {
        book::BookBuilder builder;
        builder.setMaxPly(plies);
        qint64 total = 0, done = 0;
        for (const QString &f : files)
            total += QFileInfo(f).size();
        int lastPercent = -1;
        for (const QString &f : files) {
            const qint64 before = done;
            const bool ok = builder.addPgnFile(f, nullptr, [&](qint64 pos, qint64) {
                const int percent = total > 0 ? int((before + pos) * 100 / total) : 100;
                if (percent != lastPercent) {
                    lastPercent = percent;
                    QMetaObject::invokeMethod(qApp, [self, percent, cache] {
                        if (self && self->m_chessbookKey == cache)
                            emit self->chessbookProgress(tr("reading games... %1 %").arg(percent));
                    }, Qt::QueuedConnection);
                }
                return !*cancel;
            });
            if (*cancel)
                return;
            if (ok)
                done += QFileInfo(f).size();
        }
        QDir().mkpath(QFileInfo(cache).absolutePath());
        QSaveFile out(cache);
        if (out.open(QIODevice::WriteOnly)) {
            out.write(builder.polyglotData());
            out.commit();
        }
        const int games = builder.gameCount();
        QMetaObject::invokeMethod(qApp, [self, cache, games] {
            if (!self || self->m_chessbookKey != cache)
                return;
            self->m_chessbook.open(cache);
            emit self->chessbookProgress(QString());
            emit self->statusMessage(tr("Chessbook ready: %n game(s) read.", nullptr, games), 8000);
        }, Qt::QueuedConnection);
    });
    m_chessbookThread = thread;
    connect(thread, &QThread::finished, this, [this, thread] {
        if (m_chessbookThread == thread)
            m_chessbookThread = nullptr;
        thread->deleteLater();
    });
    thread->start(QThread::LowPriority);
}

bool GameManager::playBookMove()
{
    const Position &pos = m_game.position();
    // first the opening book, then the chessbook
    Move m;
    QString source;
    if (m_book.isOpen() && pos.fullmoveNumber() <= m_settings.bookDepth)
        m = m_book.pick(pos, QRandomGenerator::global()->generate());
    if (m.isNull() && m_chessbook.isOpen() && pos.fullmoveNumber() <= m_settings.chessbookDepth) {
        m = m_chessbook.pick(pos, QRandomGenerator::global()->generate());
        for (const book::BookMove &b : m_chessbook.moves(pos)) {
            if (b.move == m) {
                const book::BookStats st = book::statsFromLearn(b.learn);
                source = tr("chessbook: %n game(s), %1 %", nullptr, st.games).arg((st.perMille + 5) / 10);
            }
        }
    }
    if (m.isNull())
        return false;
    // Played like an engine move, after a short moment so that it can be seen.
    m_engineLine = tr("Book move: %1").arg(localizeSan(toSan(pos, m), m_languages->pieceLetters()));
    if (!source.isEmpty())
        m_engineLine += QStringLiteral(" (%1)").arg(source);
    m_searchNode = m_game.currentNode();
    m_lastEval = Evaluation();
    m_lastPv.clear(); // no pondering after a book move
    const QString uci = toUci(m);
    const Game::NodeId from = m_searchNode;
    const quint64 hash = pos.hash();
    QTimer::singleShot(400, this, [this, uci, from, hash] {
        // only if nothing happened in between (new game, take back, ...)
        if (m_searchNode == from && m_game.isValidNode(from) && m_game.nodePosition(from).hash() == hash)
            onEngineBestMove(engineFor(m_game.nodePosition(from).sideToMove()), uci, QString());
    });
    emit changed();
    return true;
}
