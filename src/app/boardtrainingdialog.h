// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - Training > Board and memory: exercises without an engine, in a window of
// their own with its own board (the game on the main board stays as it is).
//
//  - Find the square: a square name is shown, the user clicks the square.
//  - Name the square: a square is marked, the user types its name.
//  - Square colour: a square name is shown on a covered board, light or dark?
//    These three are rounds of 20 squares against the clock (5 s for each mistake).
//  - Remember the position: a real position (Lichess puzzles) is shown for a few seconds,
//    then the user sets it up again with the pieces of the palettes. Six levels by the
//    number of pieces.
//  - Checks and captures / All moves: find every check and capture (every legal move) of
//    the side to move. Moves are entered on the board, nothing is played.
#pragma once

#include <chesscore/move.h>
#include <chesscore/position.h>

#include <QDialog>
#include <QElapsedTimer>
#include <QSet>
#include <QVector>

class BoardWidget;
class PiecePalette;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPushButton;
class QSpinBox;
class QTimer;

class BoardTrainingDialog : public QDialog
{
    Q_OBJECT
public:
    enum Mode { FindSquare = 0, NameSquare, SquareColor, Memory, ChecksCaptures, AllMoves, ModeCount };

    BoardTrainingDialog(Mode mode, const QString &boardTheme, const QString &pieceSet, const QString &pieceLetters,
                        QWidget *parent = nullptr);

    static QString modeName(int mode);
    // The text for the menu: the name and the best result, e.g. "Find the square (best 31.4 s)"
    static QString menuText(int mode);
    static void resetResults();

protected:
    void reject() override;

private:
    enum class Phase { Idle, Question, Pause, Show, Rebuild, Checked, Moves, Done };

    bool isSquareDrill() const { return m_mode <= SquareColor; }
    bool isMoveDrill() const { return m_mode == ChecksCaptures || m_mode == AllMoves; }
    QString settingsKey() const;   // training/board/<mode>[...]
    void updateTexts();
    void updateTime();
    void start();
    void action();                 // the second button: depends on the phase
    void stopRound();

    // square drills
    void nextQuestion();
    void answer(int square, bool light = false);
    void onNameEdited(const QString &text);
    void finishSquareRound();
    void coverBoard();

    // memory
    void showMemoryPosition();
    void startRebuild();
    void checkRebuild();
    void showOriginal(bool original);

    // moves
    void startMoves();
    void onMove(mbc::Move move);
    void finishMoves(bool gaveUp);
    QString san(const mbc::Position &pos, mbc::Move m) const;

    Mode m_mode;
    QString m_pieceLetters;
    Phase m_phase = Phase::Idle;

    BoardWidget *m_board;
    PiecePalette *m_blackPalette;
    PiecePalette *m_whitePalette;
    QLabel *m_title;
    QLabel *m_description;
    QCheckBox *m_flip = nullptr;
    QCheckBox *m_coordinates = nullptr;
    QComboBox *m_level = nullptr;
    QSpinBox *m_seconds = nullptr;
    QLabel *m_task;
    QLineEdit *m_name = nullptr;
    QPushButton *m_light = nullptr;
    QPushButton *m_dark = nullptr;
    QListWidget *m_moves = nullptr;
    QLabel *m_status;
    QLabel *m_time;
    QLabel *m_best;
    QPushButton *m_start;
    QPushButton *m_action;
    QPushButton *m_close;
    QTimer *m_tick;

    QElapsedTimer m_clock;
    qint64 m_pausedMs = 0;         // memory: time spent before the rebuild
    int m_question = 0;            // square drills: 1..20
    int m_square = mbc::NoSquare;  // the square asked for
    int m_errors = 0;

    mbc::Position m_original;      // memory and moves: the position of the task
    mbc::Position m_rebuilt;       // memory: what the user set up

    QVector<mbc::Move> m_targets;  // moves: the moves to find (one per from/to)
    QSet<int> m_found;             // indexes into m_targets
};
