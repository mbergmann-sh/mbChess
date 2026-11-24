// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - main window: the board in the middle, dock windows for clock, engine
// output, notation (with variations) and evaluation graph, menus and status bar.
// The game itself is run by the GameManager (gamemanager.h); the window shows it,
// asks the user and passes the user's actions on.
#pragma once

#include "newgamedialog.h"

#include <book/openings.h>
#include <chesscore/game.h>
#include <engine/uciengine.h>

#include <QHash>

#include <QMainWindow>

namespace mbc::dgt {
class Board;
class Scanner;
struct PortInfo;
}

class BoardWidget;
class ChessClock;
class ClockWidget;
class EvalGraphWidget;
class NotationWidget;
class QDockWidget;
class QToolBar;
class QTimer;
class SoundPlayer;
class LanguageManager;
class QAction;
class QActionGroup;
class QLabel;
class StatusField;
class QLineEdit;
class QMenu;
class QPushButton;

class QProgressBar;
class QToolButton;
class GameListWidget;
class GameManager;
class BenchmarkRunner;
class BenchmarkWindow;
class EngineAnalysis;
struct BenchmarkGame;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(LanguageManager *languages, QWidget *parent = nullptr);
    ~MainWindow() override;

    // Starts a human-vs-human game from the given position. Returns false on invalid FEN.
    bool setPositionFromFen(const QString &fen, QString *error = nullptr);
    // Opens a PGN file as game database: the game list shows all games, the first
    // one is loaded (for analysis). Shows a message on errors.
    bool openPgnFile(const QString &file);
    // Startup: is a DGT board there (stored port first, then all ports)? Runs in the
    // background; true if a search was started.
    bool dgtStartupCheck(bool quiet = true);
    bool dgtSearching() const;

protected:
    void changeEvent(QEvent *event) override;
    void closeEvent(QCloseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    void moveEvent(QMoveEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    void createUi();
    void retranslateUi();

    // Game database (PGN file with many games)
    void databaseLoaded(int count);
    void loadDatabaseGame(int index);
    void stepDatabaseGame(int step);
    void updateDatabaseActions();

    // Engines
    void manageEngines();
    void tablebaseDialog();

    // Engine analysis (menu Engines): an own engine searches the position on the board.
    void setAnalysis(bool on);
    bool analysisAllowed() const;  // not during a game against an engine or with clock
    void updateAnalysis();         // follows the position shown
    void onAnalysisUpdated();
    QString analysisText() const;
    QString analysisEnginePath() const;
    void fillAnalysisEngineMenu();

    // Engine benchmark (menu File > Benchmarks)
    void startBenchmark();
    void cancelBenchmark();
    void showBenchmarkResults();
    void updateBenchmarkActions();
    void updateBenchmarkStatus();
    // Following the running benchmark game on the board (no moves by the user meanwhile)
    void setFollowBenchmark(bool on);
    void startWatching();
    void stopWatching();
    void syncBenchmarkGame();
    void benchmarkGameFinished(const BenchmarkGame &game);

    // Game flow
    void newGameDialog();
    void startGame(const GameSettings &settings, const mbc::Position &start);
    void onBoardMove(mbc::Move move);
    void onEngineError(const QString &message);
    void onGameEnded(mbc::Result result, const QString &reason);

    // Navigation and variations
    void goToNode(mbc::Game::NodeId id);
    void goBack();
    void goForward();
    void goToStart();
    void goToEnd();
    void editComment(mbc::Game::NodeId id);

    // Files and positions
    void openPgn();
    void savePgn();
    void setFenDialog();
    void setupPosition();    // board editor
    void mateSearch();
    // Asks before a running game ends or unsaved moves/comments are lost (offers to save);
    // true if nothing is at stake or the user agrees. discards = false: the game stays (only
    // the playing ends), so unsaved moves do not matter.
    bool confirmEndGame(const QString &title, const QString &what, bool discards = true);
    // Ends a running game without a result: the board becomes an analysis board.
    void toAnalysisBoard();

    // DGT e-board (built only with Qt Serial Port)
    void setDgt(bool on);            // toolbar/menu toggle
    bool dgtWanted() const;          // switched on by the user (setting dgt/enabled)
    void dgtDialog();
    void dgtWatch();                 // ports appeared / board switched on
    QStringList dgtPortNames() const;
#ifdef MBCHESS_HAVE_DGT
    void dgtSearch(const QVector<mbc::dgt::PortInfo> &ports, bool quiet);
    void dgtSearchFinished();
#endif
    void onDgtBoardChanged();
    void updateDgtStatus();
    // not empty while a benchmark or an engine-against-engine game runs: the DGT board keeps out
    QString dgtPauseReason() const;
    // general message in the "Status:" section of the status bar; 0 ms: stays
    void showStatus(const QString &text, int timeout = 0);
    void updateDgtIcon();
    void dgtTakePosition();          // board editor with the pieces of the DGT board
    void copyFen();
    void enterMove();
    void flipBoard();
    void showAbout();
    void showManual();   // F1: .chm on Windows if there, else the PDF in the system viewer
    QString manualFile() const;
    void updateView();
    QString statusText() const;
    void restoreWindowState();
    void saveWindowState();
    void createToolBar();
    void updateOpeningName();
    void updateToolBarIcons();
    void updateSoundIcons(); // the icons show whether sounds / announcements are on
    bool confirmQuit();

    // Themes and sounds
    void createThemeMenus();
    void retranslateThemeMenus();
    QString themeDisplayName(const QString &id) const; // "" if not a built-in name
    void updateSpeechLanguage();

    LanguageManager *m_languages;
    // The game itself (moves, engines, clock, books) lives in the GameManager; m_game is
    // a short name for m_gm->game(), which the view widgets show.
    GameManager *m_gm;
    mbc::Game &m_game;
    StatusField *m_chessbookLabel = nullptr;  // progress in the status bar
    mbc::book::OpeningIndex m_openings;  // names of openings, loaded when first needed
    bool m_openingsLoaded = false;
    QHash<QString, QString> m_openingNamesDe;  // English name -> German name
    StatusField *m_openingLabel = nullptr;    // in the status bar

    QMenu *m_fileMenu = nullptr;
    QMenu *m_gameMenu = nullptr;
    QMenu *m_boardMenu = nullptr;
    QMenu *m_viewMenu = nullptr;
    QMenu *m_boardThemeMenu = nullptr;
    QMenu *m_pieceSetMenu = nullptr;
    QActionGroup *m_boardThemeGroup = nullptr;
    QActionGroup *m_pieceSetGroup = nullptr;
    QAction *m_effectsAction = nullptr;
    QAction *m_speechAction = nullptr;
    SoundPlayer *m_sound = nullptr;
    QMenu *m_voiceMenu = nullptr;
    QActionGroup *m_voiceGroup = nullptr;
    QToolBar *m_toolBar = nullptr;
    QTimer *m_saveStateTimer = nullptr;
    bool m_windowRestored = false;
    QMenu *m_languageMenu = nullptr;
    QMenu *m_helpMenu = nullptr;
    QAction *m_newGameAction = nullptr;
    QAction *m_openAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_setFenAction = nullptr;
    QAction *m_copyFenAction = nullptr;
    QAction *m_quitAction = nullptr;
    QAction *m_takeBackAction = nullptr;
    QAction *m_moveNowAction = nullptr;
    QAction *m_resignAction = nullptr;
    QAction *m_backAction = nullptr;
    QAction *m_forwardAction = nullptr;
    QAction *m_startAction = nullptr;
    QAction *m_endAction = nullptr;
    QAction *m_resetLayoutAction = nullptr;
    QMenu *m_guiThemeMenu = nullptr;
    QActionGroup *m_guiThemeGroup = nullptr;
    QAction *m_flipAction = nullptr;
    QAction *m_coordinatesAction = nullptr;
    QAction *m_germanAction = nullptr;
    QAction *m_englishAction = nullptr;
    QAction *m_aboutAction = nullptr;
    QAction *m_manualAction = nullptr;
    QAction *m_aboutQtAction = nullptr;
    QActionGroup *m_languageGroup = nullptr;

    BoardWidget *m_board = nullptr;
    ClockWidget *m_clockWidget = nullptr;
    QLabel *m_engineTitle = nullptr;
    QLabel *m_engineOutput = nullptr;
    NotationWidget *m_notation = nullptr;
    EvalGraphWidget *m_graph = nullptr;
    QDockWidget *m_clockDock = nullptr;
    QDockWidget *m_engineDock = nullptr;
    QDockWidget *m_notationDock = nullptr;
    QDockWidget *m_graphDock = nullptr;
    QDockWidget *m_gameListDock = nullptr;
    GameListWidget *m_gameList = nullptr;
    QAction *m_prevGameAction = nullptr;
    QMenu *m_engineMenu = nullptr;
    QAction *m_manageEnginesAction = nullptr;
    QAction *m_tablebaseAction = nullptr;
    QAction *m_analysisAction = nullptr;
    QAction *m_setupPositionAction = nullptr;
    QAction *m_mateSearchAction = nullptr;
    QAction *m_splashAction = nullptr;
    QAction *m_dgtAction = nullptr;
    QAction *m_dgtSettingsAction = nullptr;
    QAction *m_dgtTakeAction = nullptr;
    mbc::dgt::Board *m_dgt = nullptr;
    mbc::dgt::Scanner *m_dgtScanner = nullptr;
    StatusField *m_dgtStatus = nullptr;
    StatusField *m_statusField = nullptr;   // "Status:" general messages
    QTimer *m_statusTimer = nullptr;
    QString m_statusMessage;
    QTimer *m_dgtWatcher = nullptr;
    QStringList m_dgtKnownPorts;
    int m_dgtWatchTicks = 0;
    bool m_dgtUserOff = false;       // switched off by the user in this session
    bool m_dgtDialogOpen = false;
    QString m_dgtFoundPort;
    bool m_dgtQuiet = true;   // the board holds a position set up by the user
    QMenu *m_analysisEngineMenu = nullptr;
    QMenu *m_analysisLinesMenu = nullptr;
    QActionGroup *m_analysisEngineGroup = nullptr;
    QActionGroup *m_analysisLinesGroup = nullptr;
    EngineAnalysis *m_analysis = nullptr;
    mbc::Game::NodeId m_analysisNode = -1; // where the analysis shown was made
    QMenu *m_benchmarkMenu = nullptr;
    QAction *m_benchmarkStartAction = nullptr;
    QAction *m_benchmarkCancelAction = nullptr;
    QAction *m_benchmarkResultsAction = nullptr;
    QAction *m_benchmarkFollowAction = nullptr;
    bool m_watching = false;          // the board shows the running benchmark game
    int m_watchIndex = -1;            // which benchmark game
    QString m_watchTitle;             // "Benchmark: A vs. B (3/56)"
    BenchmarkRunner *m_benchmark = nullptr;
    BenchmarkWindow *m_benchmarkWindow = nullptr;
    QWidget *m_benchmarkStatus = nullptr;
    QLabel *m_benchmarkLabel = nullptr;
    QProgressBar *m_benchmarkProgress = nullptr;
    QToolButton *m_benchmarkCancelButton = nullptr;
    QTimer *m_benchmarkTimer = nullptr; // updates the remaining time in the status bar
    QAction *m_nextGameAction = nullptr;
    QByteArray m_defaultLayout;
    QLabel *m_inputLabel = nullptr;
    QLineEdit *m_input = nullptr;
    QPushButton *m_moveButton = nullptr;
    QLabel *m_status = nullptr;
    QLabel *m_fen = nullptr;
};
