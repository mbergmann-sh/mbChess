// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the game being played: the game tree, the settings it was started with,
// the engines (one, or two in "engine against engine"), the clock, the opening book and
// the chessbook. GameManager runs the game (whose turn, engine searches, book moves,
// game end, take back, resign) and tells the window through signals what to show.
//
// Two ways of playing: in a "playing" game (against an engine, or with a clock) moves
// are made only at the end of the game; one can look back, but not branch off. Without
// engine and clock ("analysis") a move anywhere in the game adds a variation.
#pragma once

#include "newgamedialog.h"

#include <book/polyglot.h>
#include <chesscore/game.h>
#include <engine/uciengine.h>

#include <QObject>

#include <atomic>
#include <memory>

class ChessClock;
class LanguageManager;
class QThread;

class GameManager : public QObject
{
    Q_OBJECT
public:
    explicit GameManager(LanguageManager *languages, QObject *parent = nullptr);
    ~GameManager() override;

    // --- State ---
    mbc::Game &game() { return m_game; }
    const mbc::Game &game() const { return m_game; }
    const GameSettings &settings() const { return m_settings; }
    ChessClock *clock() const { return m_clock; }
    mbc::engine::UciEngine *engine() const { return m_engine; }
    // The engine that plays this colour (in "engine against engine" Black has its own).
    mbc::engine::UciEngine *engineFor(mbc::Color c) const;
    // A name the user gave the engine in "Manage engines", else what the engine reports.
    QString engineDisplayName(const mbc::engine::UciEngine *engine) const;
    bool anyEngineThinking() const;
    bool anyEngineStarting() const;
    bool isSearching() const { return m_searchNode >= 0; }

    bool isGameOver() const { return m_gameOver; }
    QString endReason() const { return m_endReason; }
    void setGameOver(bool over, const QString &reason = QString());
    // After start-up, until a game is started or opened: no moves on the board.
    bool isWaitingForGame() const { return m_waitingForGame; }
    void setWaitingForGame(bool waiting) { m_waitingForGame = waiting; }
    // The board holds a position from the board editor or a FEN.
    bool isPositionSetUp() const { return m_positionSetUp; }
    // Suspended while the window shows a game from elsewhere (the running benchmark):
    // no moves by the user, no engine searches, the clock decides nothing.
    bool isSuspended() const { return m_suspended; }
    void setSuspended(bool suspended) { m_suspended = suspended; }

    // Engine output for the engine panel (score, depth, main line, or the book move).
    QString engineLine() const { return m_engineLine; }
    void setEngineLine(const QString &line) { m_engineLine = line; }
    // A search info line as text; *eval gets the score from White's view.
    QString engineInfoText(const mbc::Position &root, mbc::Color engineSide, const mbc::engine::SearchInfo &info,
                           mbc::Evaluation *eval) const;

    bool isPlayingMode() const;   // engine or clock: moves only at the end of the game
    bool isEngineTurn() const;
    bool humanMayMove() const;
    // A game is running (an engine or the clock plays, at least one move made).
    bool gameInProgress() const;
    mbc::Game::NodeId mainLineEnd() const;
    static QString statusReason(mbc::GameStatus status);

    // Unsaved data: moves or comments not in a file since loading or saving.
    void markSaved();
    bool hasUnsavedChanges() const;

    // --- Starting and ending ---
    void startGame(const GameSettings &settings, const mbc::Position &start);
    // A game from a file, human against human (for analysis).
    void loadGame(const mbc::Game &game);
    // A position from the board editor or a FEN, human against human.
    void usePosition(const mbc::Position &pos);
    // Ends the playing without a result: engines and clock stop, moves add variations.
    void stopPlaying();
    void endGame(mbc::Result result, const QString &reason);
    void updatePlayerNames();

    // --- Moves ---
    void applyMove(mbc::Move move);       // from board or keyboard
    void takeBack();
    void moveNow();
    void resign();
    void promoteVariation(mbc::Game::NodeId id);
    void deleteVariation(mbc::Game::NodeId id);
    // After moves were added or removed: a different main line end has no result yet.
    void afterTreeChange(mbc::Game::NodeId oldMainEnd);

    // Engines need a real file: the built-in book copied out of the program.
    static QString builtInBookFile();

signals:
    void changed();                                  // the window shows the game again
    void engineOutputChanged(const QString &line);   // new search info of the playing engine
    void movePlayed(const QString &san);             // for the move sounds
    void gameEnded(mbc::Result result, const QString &reason);
    void playerNamesChanged(const QString &white, const QString &black);
    void engineError(const QString &message);
    void statusMessage(const QString &text, int timeoutMs);
    void chessbookProgress(const QString &text);     // empty: no chessbook is being built

private:
    void connectEngine(mbc::engine::UciEngine *engine);
    void prepareEngine(mbc::engine::UciEngine *engine, const QString &path, QString *bookSetupState,
                       QString *displayName);
    void applyUserEngineOptions(mbc::engine::UciEngine *engine);
    void applyEngineBookOptions(mbc::engine::UciEngine *engine);
    void onEngineReady(mbc::engine::UciEngine *engine);
    void onEngineBestMove(mbc::engine::UciEngine *engine, const QString &move);
    void onEngineInfo(mbc::engine::UciEngine *engine, const mbc::engine::SearchInfo &info);
    void onFlagFell(mbc::Color side);
    void startEngineSearch();
    bool playBookMove();
    void afterMove();
    void checkGameEnd();
    void loadBook();
    void loadChessbook();

    LanguageManager *m_languages;
    mbc::Game m_game;
    GameSettings m_settings;
    bool m_gameOver = false;
    QString m_endReason;
    bool m_waitingForGame = true;
    bool m_positionSetUp = false;
    bool m_suspended = false;
    QString m_savedPgn;                   // PGN text at the last save or load

    mbc::engine::UciEngine *m_engine = nullptr;
    mbc::engine::UciEngine *m_engine2 = nullptr; // Black in "engine against engine"
    QString m_engineBookSetup;           // book options the running engine was started with
    QString m_engine2BookSetup;
    QString m_engineDisplayName;
    QString m_engine2DisplayName;
    ChessClock *m_clock = nullptr;
    QString m_engineLine;
    mbc::Game::NodeId m_searchNode = -1;  // where the running engine search started
    mbc::Evaluation m_lastEval;           // last score of that search (White's view)

    mbc::book::PolyglotBook m_book;      // opening book for the engine's first moves
    mbc::book::PolyglotBook m_chessbook; // built from game databases (chessbooks)
    QString m_chessbookKey;              // cache file of the chessbook wanted now
    QThread *m_chessbookThread = nullptr;
    std::shared_ptr<std::atomic<bool>> m_chessbookCancel;
};
