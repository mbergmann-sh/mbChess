// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - engine benchmark: a round robin tournament between the installed engines
// under equal conditions (same time control, threads, hash, no own books, same openings
// with both colours). Plays one game at a time, each with freshly started engines.
#pragma once

#include "enginemanager.h"

#include <chesscore/game.h>
#include <engine/uciengine.h>

#include <QElapsedTimer>
#include <QObject>
#include <QPointer>
#include <QVector>

class QTimer;

struct BenchmarkSettings
{
    QVector<EngineEntry> engines;
    qint64 baseMs = 5 * 60 * 1000;   // 5 minutes
    qint64 incrementMs = 3000;       // + 3 seconds per move
    int gamesPerPairing = 2;         // even: every opening with both colours
    int threads = 1;
    int hashMb = 64;
    int elo = 0;                     // 0 = full strength, otherwise limited where the engine can
    QString tablebasePath;           // Syzygy folder for the engines that can use it, empty: none
    bool followOnBoard = true;       // show the running game on the main board (not used by the runner)
    bool showResultsAtEnd = true;    // open the results window after a complete run (not used by the runner)
};

struct BenchmarkGame
{
    int white = -1;                  // index into BenchmarkSettings::engines
    int black = -1;
    mbc::Result result = mbc::Result::Unknown;
    QString reason;                  // "Checkmate", "Time forfeit", ...
    QString opening;                 // name of the start line
    int plies = 0;
    qint64 durationMs = 0;           // wall clock time of the game
    qint64 thinkMs[2] = { 0, 0 };    // time used by White, Black
    int moves[2] = { 0, 0 };         // moves played by White, Black (without the opening)
    mbc::Game game;                  // for saving as PGN
};

class BenchmarkRunner : public QObject
{
    Q_OBJECT
public:
    explicit BenchmarkRunner(QObject *parent = nullptr);
    ~BenchmarkRunner() override;

    void start(const BenchmarkSettings &settings);
    void cancel();
    bool isRunning() const { return m_running; }

    const BenchmarkSettings &settings() const { return m_settings; }
    const QVector<BenchmarkGame> &games() const { return m_games; }
    int totalGames() const { return int(m_schedule.size()); }
    qint64 elapsedMs() const { return m_total.isValid() ? m_total.elapsed() : 0; }

    // Expected time until the end: from the games played so far once there are some,
    // before that from the time control. -1 when not running.
    qint64 remainingMs() const;
    QString currentWhite() const { return m_inGame ? m_players[0].name : QString(); }
    QString currentBlack() const { return m_inGame ? m_players[1].name : QString(); }
    int currentIndex() const { return m_index; }

    // The running game (moves and evaluations so far), for following it on the board.
    bool isInGame() const { return m_inGame; }
    const mbc::Game &currentGame() const { return m_current.game; }
    const QString &currentOpening() const { return m_current.opening; }
    int currentOpeningPlies() const { return m_openingPlies; }
    // Clock of White (0) or Black (1), counting down while that side thinks.
    qint64 clockMs(int side) const;
    // The side whose clock runs (0/1), -1 while the engines start or between games.
    int thinkingSide() const;
    // Pause between two games (default 300 ms), e.g. to look at the end of a game.
    void setPauseBetweenGames(int ms) { m_pauseMs = ms; }

    // Rough upper estimate for the whole tournament.
    static qint64 estimatedMs(const BenchmarkSettings &s);
    static int gameCount(const BenchmarkSettings &s);

signals:
    // A game starts (index from 0).
    void gameStarted(int index, int total, const QString &white, const QString &black);
    void gameFinished(const BenchmarkGame &game);
    void finished(bool cancelled);
    // The running game changed: opening set up, the engines are ready, or a move was made.
    void positionChanged();
    // Search output of the engine playing White (0) or Black (1).
    void searchInfo(int side, const mbc::engine::SearchInfo &info);

private:
    struct Pairing
    {
        int white;
        int black;
        int opening;
    };
    struct Player
    {
        QPointer<mbc::engine::UciEngine> engine;
        bool ready = false;
        QString name;
    };

    void startGame();
    void onReady(int side);
    void requestMove();
    void onBestMove(int side, const QString &move);
    void onEngineError(int side, const QString &message);
    void onTimeout();
    void endGame(mbc::Result result, const QString &reason);
    void stopEngines();
    void applyOptions(mbc::engine::UciEngine *engine, const EngineEntry &entry);

    BenchmarkSettings m_settings;
    QVector<Pairing> m_schedule;
    QVector<BenchmarkGame> m_games;
    int m_index = -1;
    bool m_running = false;
    bool m_inGame = false;

    Player m_players[2];               // 0 = White, 1 = Black of the current game
    BenchmarkGame m_current;
    qint64 m_remaining[2] = { 0, 0 };
    QElapsedTimer m_moveTimer;
    QElapsedTimer m_gameTimer;
    QElapsedTimer m_total;
    QTimer *m_flagTimer = nullptr;     // time forfeit when an engine does not answer
    QTimer *m_startTimer = nullptr;    // engines that never get ready
    int m_openingPlies = 0;
    int m_pauseMs = 300;
    bool m_searching = false;          // the clock of the side to move runs
    mbc::Evaluation m_lastEval;        // of the running search, from White's view
};
