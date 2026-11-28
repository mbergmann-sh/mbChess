// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - menu Play > Rated game: the player has an Elo rating of their own in mbChess
// (1200 at the start). A rated game is played against an engine held to that rating
// (UCI_Elo; Texel first, it is calibrated down to beginner strength), and its result
// changes the rating by the Elo formula (K = 40 for the first 20 games, then 20).
// Take-backs and the tutor are off in a rated game; a game that is left unfinished
// (new game, program closed) counts as lost.
#pragma once

#include "newgamedialog.h"

#include <chesscore/position.h>

#include <QDialog>
#include <QString>
#include <QVector>

class QComboBox;
class QLabel;
class QListWidget;

namespace rated {

struct Record
{
    QString date;        // ISO
    QString opponent;    // "Texel"
    int opponentElo = 0;
    bool white = true;   // the player's colour
    double score = 0;    // 1, 0.5, 0
    int change = 0;
    int eloAfter = 0;
    bool abandoned = false;
};

int elo();
int games();
QVector<Record> history();   // newest first
void reset();

// Settings for a rated game; false with *error if there is no engine.
bool prepare(GameSettings::ColorChoice color, const TimeControl &tc, GameSettings *settings, QString *error);
// The game has started (it counts as lost until finish()).
void begin(const GameSettings &settings);
// The result for the player; returns the record (with the rating change).
Record finish(double score, bool abandoned = false);
// A rated game that was never finished (program closed): it is counted as lost now.
bool resolvePending(Record *record);
bool hasPending();

QString scoreText(double score);   // "1-0" style from the player's view: "won", "draw", "lost"

} // namespace rated

class RatedGameDialog : public QDialog
{
    Q_OBJECT
public:
    explicit RatedGameDialog(QWidget *parent = nullptr);
    GameSettings settings() const { return m_settings; }

    // Shown after a rated game (and for a game counted as lost)
    static QString resultText(const rated::Record &r);

private:
    void updateTexts();
    void startGame();

    QLabel *m_elo;
    QLabel *m_stats;
    QListWidget *m_history;
    QComboBox *m_color;
    QComboBox *m_time;
    GameSettings m_settings;
};
