// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the tutor (menu Play): in a game against the computer an engine of its own
// looks at every move of the player before it is played. While the player thinks, the
// tutor searches the position (MultiPV); after the move it searches the new position
// for a moment if the move was not among its own lines. It rates the move by how much it
// lowers the player's winning chances (as Lichess does: from the centipawns): 5 points
// (of 100) inaccuracy, 10 mistake, 15 blunder, from 35 a particularly stupid move (a
// queen or a mate given away). From a mistake on, a move that loses 6 pawns or more or
// lets the opponent mate is such a move too (in a position that was lost or won before,
// the winning chances fall less).
// If the move is worse than the level chosen in the menu, the window asks the player
// (TutorDialog) whether to take it back, play the tutor's move or play it anyway.
#pragma once

#include <chesscore/game.h>
#include <chesscore/move.h>
#include <chesscore/position.h>

#include <QElapsedTimer>
#include <QObject>
#include <QStringList>

class EngineAnalysis;
class QTimer;
struct EngineEntry;

class Tutor : public QObject
{
    Q_OBJECT
public:
    enum Severity { Good = 0, Inaccuracy, Mistake, Blunder, Stoned };

    struct Verdict
    {
        mbc::Move move;               // the player's move
        mbc::Move best;               // the tutor's move (null: unknown)
        mbc::Evaluation before;       // the best evaluation before the move (White's view)
        mbc::Evaluation after;        // after the player's move
        QStringList bestLine;         // UCI, from the position before the move
        QStringList reply;            // UCI, the opponent's best answer and what follows
        double drop = 0;              // lost winning chances, points of 0..100
        Severity severity = Good;
        QString fen;                  // the position before the move
    };

    explicit Tutor(QObject *parent = nullptr);
    ~Tutor() override;

    // Menu Play > Tutor: on or off, and from which severity on it speaks up
    static bool isEnabled();
    static void setEnabled(bool on);
    static Severity level();
    static void setLevel(Severity s);
    static QString severityName(Severity s);

    // The position on the board: active = the player is to move in a tutor game; the
    // tutor then starts to search it, otherwise it pauses.
    void setPosition(bool active, const EngineEntry &engine, const mbc::Position &start, const QStringList &moves);
    void stop();                       // quits the engine (end of the game)
    // Rates this move of the player (a legal move in the position of setPosition()):
    // verdictReady() follows, in about a second.
    void check(mbc::Move move);
    bool isChecking() const { return m_phase != Phase::Idle; }
    void cancel();

    static double winChance(const mbc::Evaluation &e, mbc::Color side); // 0..100 for side

signals:
    void verdictReady(const Tutor::Verdict &verdict);
    void errorOccurred(const QString &message);

private:
    enum class Phase { Idle, Before, After };
    void poll();
    void finish();
    bool lineUsable(int index) const;

    EngineAnalysis *m_analysis;
    QTimer *m_poll;
    QElapsedTimer m_searchClock;  // since the search of the current position started
    QElapsedTimer m_waitClock;    // since check()
    Phase m_phase = Phase::Idle;
    bool m_active = false;
    mbc::Position m_start;
    QStringList m_moves;
    QString m_searching;          // the position the engine searches for setPosition()
    mbc::Position m_position;     // before the player's move
    Verdict m_verdict;
};
