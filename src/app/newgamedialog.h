// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - "New game" dialog: game mode, engine, strength, cores, hash, time control.
#pragma once

#include "chessclock.h"

#include <QDialog>
#include <QMap>

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;

struct GameSettings
{
    enum Mode { HumanVsHuman = 0, HumanWhiteVsEngine, HumanBlackVsEngine, EngineVsEngine };

    enum ColorChoice { PlayWhite = 0, PlayBlack, PlayRandom };

    Mode mode = HumanWhiteVsEngine;
    ColorChoice colorChoice = PlayWhite; // what the player picked; "random" is resolved into mode
    QString playerName;
    QString enginePath;       // the engine; in "engine against engine" the one with White
    QString engine2Path;      // "engine against engine": the engine with Black
    int strengthElo = 1600;   // 0 = full strength
    int threads = 1;
    int hashMb = 128;
    TimeControl timeControl;
    bool autoQueen = false;   // promote to a queen without asking
    // Opening book of mbChess (PolyGlot .bin): the engine's first moves come from it.
    bool useBook = true;
    QString bookFile;         // empty: the built-in book
    int bookDepth = 12;       // up to this full move
    // Chessbooks: game databases (PGN), turned into a book with the results of the games.
    bool useChessbook = true;
    QString chessbookFile = QStringLiteral("*"); // "*": all PGN files in the Chessbooks folder
    int chessbookDepth = 20;
    // The engine's own book (UCI options OwnBook / BookFile / BookPath), if it has one.
    bool engineOwnBook = true;
    QString engineBookFile;   // empty: the engine's default, PolyGlot engines get mbChess' book
    // The engine thinks on the human's time (UCI "go ponder"), if it can. Only against a
    // human and at full strength.
    bool ponder = false;
    // Set by the quick start of the menu "Play" (not stored): the opponent's name and
    // approximate Elo, extra UCI options, or a player built into mbChess (no engine).
    // BuiltinTrainer: the defender of a training task (menu Training), see Trainer.
    enum Builtin { NoBuiltin = 0, BuiltinRandom, BuiltinCapturer, BuiltinTrainer };
    QString opponentName;
    int opponentElo = 0;
    int builtinPlayer = NoBuiltin;
    QMap<QString, QString> engineOptions;
    // Without a clock: the engine's time per move in ms (0: 1.5 s when held down, else 3 s).
    int moveTimeMs = 0;

    bool hasEngine() const { return mode != HumanVsHuman; }
    bool engineVsEngine() const { return mode == EngineVsEngine; }
    // The engine's colour against a human (not used in "engine against engine").
    mbc::Color engineColor() const { return mode == HumanWhiteVsEngine ? mbc::Black : mbc::White; }
    // True if an engine plays this colour.
    bool isEngineSide(mbc::Color c) const { return engineVsEngine() || (hasEngine() && c == engineColor()); }

    static GameSettings load();   // from QSettings
    void save() const;
    static QString detectEngine(); // from the program's "engines" folder, Stockfish preferred
};

class NewGameDialog : public QDialog
{
    Q_OBJECT
public:
    explicit NewGameDialog(const GameSettings &settings, QWidget *parent = nullptr);
    GameSettings settings() const;
    // Offers "play from the position on the board" (only shown after this call).
    void offerBoardPosition(bool checked);
    bool fromBoardPosition() const;
    // Preselects "engine against engine" (menu Play).
    void setEngineVsEngine();
    // Preselects a game against the engine (menu Play > Game with tutor).
    void setAgainstEngine();

    struct Level { int elo; const char *name; };
    static const Level kLevels[];
    static int levelCount();
    static QString levelName(int elo);

protected:
    void accept() override;

private:
    void updateEnabled();
    void browseEngine();
    void fillEngines(const QString &select);
    void selectEngine(const QString &path);
    void fillFileCombo(QComboBox *combo, const QString &folder, const QString &pattern,
                       const QString &firstText, const QString &firstData, const QString &current);

    QComboBox *m_mode;     // opponent: human or engine
    QComboBox *m_color;    // the player's colour against the engine
    QLineEdit *m_name;
    QComboBox *m_engine;
    QLabel *m_engineLabel;
    QComboBox *m_engine2;  // "engine against engine": Black
    QLabel *m_engine2Label;
    QPushButton *m_browse;
    QComboBox *m_strength;
    QSpinBox *m_threads;
    QSpinBox *m_hash;
    QComboBox *m_time;
    QLabel *m_engineHint;
    QCheckBox *m_autoQueen;
    QCheckBox *m_ponder;
    QCheckBox *m_fromPosition;
    QCheckBox *m_useBook;
    QComboBox *m_bookCombo;
    QSpinBox *m_bookDepth;
    QCheckBox *m_useChessbook;
    QComboBox *m_chessbookCombo;
    QSpinBox *m_chessbookDepth;
    QLabel *m_chessbookHint;
    QCheckBox *m_ownBook;
    QLineEdit *m_engineBookFile;
    QPushButton *m_engineBookBrowse;
};
