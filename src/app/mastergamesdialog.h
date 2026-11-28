// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - Training > Master games: guess the moves of a master. A game from a PGN file
// (some famous games are built in, any file of your own works) is played on a board of
// its own; at each move of the chosen side the user guesses the master's move. The
// master's move gives 10 points; any other move is rated by the analysis engine against
// the master's move (lost winning chances, as the tutor does) and gives 0 to 8 points.
// A hint (the piece that moves) halves the points of that move. The game on the main
// board stays as it is.
#pragma once

#include "enginemanager.h"

#include <chesscore/game.h>
#include <chesscore/move.h>
#include <chesscore/position.h>

#include <QDialog>
#include <QElapsedTimer>
#include <QVector>

class BoardWidget;
class EngineAnalysis;
class QComboBox;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;

class MasterGamesDialog : public QDialog
{
    Q_OBJECT
public:
    MasterGamesDialog(const EngineEntry &engine, const QString &boardTheme, const QString &pieceSet,
                      const QString &pieceLetters, QWidget *parent = nullptr);
    ~MasterGamesDialog() override;

    static QString builtinFile();   // ":/training/masters/classics.pgn"

protected:
    void reject() override;

private:
    enum class Phase { Idle, Replay, Guess, Rating, Done };

    void loadFile(const QString &path);
    void chooseFile();
    void onGameChosen(int index);
    QString gameName(const mbc::Game &g) const;
    QString settingsKey() const;    // training/masters/<file and game>/<side>
    void updateBest();
    void updateButtons();

    void start();
    void stopRound();
    void step();                    // the next move of the game, or the next guess
    void onMove(mbc::Move move);
    void hint();
    void showMove();                // the master's move, no points
    void poll();                    // the engine rates the guess
    void scoreGuess(int points, bool found, const QString &note);
    void playMasterMove();
    void finishGame();

    QString san(const mbc::Position &pos, mbc::Move m) const;
    QString moveNumber(int ply) const;   // "12." or "12..."
    QString evalText(const mbc::Evaluation &e) const;   // from the user's view
    bool userPly(int ply) const;

    QString m_pieceLetters;
    EngineEntry m_engine;
    EngineAnalysis *m_analysis = nullptr;
    QTimer *m_poll;
    QTimer *m_replay;

    BoardWidget *m_board;
    QComboBox *m_games;
    QPushButton *m_file;
    QComboBox *m_side;
    QSpinBox *m_fromMove;
    QSpinBox *m_seconds;
    QLabel *m_info;
    QLabel *m_task;
    QLabel *m_status;
    QListWidget *m_list;
    QLabel *m_score;
    QLabel *m_best;
    QPushButton *m_start;
    QPushButton *m_hint;
    QPushButton *m_show;
    QPushButton *m_close;

    QString m_path;
    QVector<mbc::Game> m_gameList;
    Phase m_phase = Phase::Idle;
    mbc::Color m_userColor = mbc::White;
    QVector<mbc::Move> m_moves;          // the main line of the game
    QVector<mbc::Position> m_positions;  // before each move (m_positions[i] before m_moves[i])
    int m_ply = 0;                       // the next move of the game
    int m_firstPly = 0;                  // the first ply that is guessed
    int m_points = 0;
    int m_maxPoints = 0;
    int m_guessed = 0;                   // the master's move found
    bool m_hinted = false;               // a hint for the current move

    // rating: first the position after the guess, then after the master's move
    mbc::Move m_guess;
    int m_ratingStep = 0;                // 0 guess, 1 master move
    mbc::Evaluation m_guessEval;
    mbc::Evaluation m_masterEval;
    QElapsedTimer m_searchClock;
};
