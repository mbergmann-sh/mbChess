// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the menu "Training": tasks played on the main board.
//
// "Find the mate": mate in 1, 2 or 3 moves. The puzzles come from the Lichess puzzle
// database (CC0) and from mbChess itself (resources/training/mate<N>.txt), more can be put
// into the folder "Training" next to the program (the same format, or Lichess CSV files).
// mbChess plays the defence itself (GameSettings::BuiltinTrainer): the exact mate solver
// of chesscore checks each move of the user and finds the most stubborn reply.
//
// "Endgames": basic mates (KQK, KRK, KBBK, KBNK) and king and pawn against king, from
// random positions, against an engine. The goal ("mate in at most 15 moves") is checked
// with the Gaviota tablebases, if they are installed, and the KPK table of chesscore.
#pragma once

#include "newgamedialog.h"

#include <chesscore/game.h>
#include <chesscore/matesolver.h>
#include <chesscore/position.h>

#include <QObject>
#include <QStringList>
#include <QVector>

class GameManager;
class LanguageManager;

namespace training {

struct Puzzle
{
    QString id;          // "lichess:00sHx", "mbchess:boden", "file:<name>:<line>"
    QString fen;         // the position before firstMove
    QString firstMove;   // the opponent's last move (UCI), empty: the task starts at fen
    QStringList solution; // UCI moves, from the position after firstMove
    int rating = 0;
};

// All puzzles "mate in n" (1..3): built in, then the files of the folder "Training".
const QVector<Puzzle> &matePuzzles(int n);
QString trainingFolder();   // "Training" next to the program

} // namespace training

class Trainer : public QObject
{
    Q_OBJECT
public:
    enum class Kind { None, Mate, Endgame };
    enum Endgame { QueenMate = 0, RookMate, TwoBishops, BishopKnight, PawnWin, PawnDraw, EndgameCount };
    enum class State { Running, Solved, Failed };

    Trainer(GameManager *gm, LanguageManager *languages, QObject *parent = nullptr);

    Kind kind() const { return m_kind; }
    State state() const { return m_state; }
    int mateMoves() const { return m_mateMoves; }
    int endgame() const { return m_endgame; }
    // The training game is on the board and not decided yet.
    bool isRunning() const;
    // The task is still the game on the board (running or finished).
    bool isOnBoard() const;

    // Prepare a task; the window then starts the game with startGame(*settings, *start)
    // and calls started(). False with *error if there is none.
    bool prepareMate(int n, GameSettings *settings, mbc::Position *start, QString *error);
    bool prepareEndgame(int type, GameSettings *settings, mbc::Position *start, QString *error);
    // The same task once more.
    bool prepareAgain(GameSettings *settings, mbc::Position *start, QString *error);
    void started();
    void stop();

    // A move of the user. False: refused (not a move of the solution), nothing is played.
    bool userMove(mbc::Move move);
    // The user wants help: a hint, the solution (the task then counts as not solved).
    QString hint();
    QString solution();

    // Texts for the panel "Training"
    QString title() const;
    QString task() const;
    QString source() const;      // where the puzzle comes from (with a link), may be empty
    QString progress() const;
    QString message() const { return m_message; }

    static QString endgameName(int type);
    static QString mateMenuText(int n);   // "Mate in 2 (12 of 719 solved)"
    static void resetMateProgress();

signals:
    void changed();             // texts or state changed
    void finished(bool solved);

private:
    mbc::Move defenderMove(const mbc::Position &pos);
    void onGameChanged();
    void onGameEnded(mbc::Result result, const QString &reason);
    void finish(bool solved, const QString &message);
    int userMovesDone() const;   // complete moves of the user in the task so far
    mbc::Color userColor() const { return m_userColor; }
    GameSettings baseSettings() const;
    QString san(const mbc::Position &pos, mbc::Move m) const;
    QString sanLine(const mbc::Position &pos, const QStringList &uci) const;
    QStringList mainLine(const mbc::Position &pos, int moves);
    void checkEndgameMove(const mbc::Position &before, mbc::Move move, const mbc::Position &after);
    QString enginePath(QString *name) const;
    bool generate(int type, mbc::Position *pos) const;

    GameManager *m_gm;
    LanguageManager *m_languages;
    Kind m_kind = Kind::None;
    State m_state = State::Running;
    int m_serial = -1;            // GameManager::gameSerial() of the task
    mbc::Color m_userColor = mbc::White;
    mbc::Position m_start;        // the game starts here (before the opponent's first move)
    mbc::Position m_taskPosition; // the user moves first here
    int m_offset = 0;             // plies before the user's first move (0 or 1)
    QString m_message;
    int m_mistakes = 0;
    bool m_helped = false;        // the solution was shown
    int m_lastPly = 0;            // the last ply looked at by onGameChanged

    // Find the mate
    int m_mateMoves = 0;
    int m_puzzleIndex = -1;
    training::Puzzle m_puzzle;
    mbc::MateSolver m_solver;

    // Endgames
    int m_endgame = 0;
    int m_limit = 0;              // moves
    int m_startDtm = 0;           // Gaviota: mate in this many moves at the start (0: unknown)
    bool m_tablebase = false;     // Gaviota knows the position
};
