// SPDX-License-Identifier: GPL-3.0-or-later
#include "mainwindow.h"

#include "gamemanager.h"

#include "benchmark.h"
#include "benchmarkdialog.h"
#include "benchmarkwindow.h"
#include "boardeditordialog.h"
#include "matesearchdialog.h"
#include "opponents.h"
#include "splashscreen.h"
#include "statusfield.h"
#ifdef Q_OS_WIN
#include "htmlhelp.h"
#endif

#include "mbchess_codename.h"
#ifdef MBCHESS_HAVE_DGT
#include "dgtdialog.h"
#include <dgt/dgtboard.h>
#include <dgt/dgtscanner.h>
#endif
#include "engineanalysis.h"
#include "tablebasedialog.h"
#include "tablebases.h"

#include "boardwidget.h"
#include "chessclock.h"
#include "enginedialog.h"
#include "enginemanager.h"
#include "evalgraphwidget.h"
#include "gamelistwidget.h"
#include "guitheme.h"
#include "languagemanager.h"
#include "notationwidget.h"
#include "soundplayer.h"
#include "themes.h"

#include <chesscore/notation.h>
#include <pgn/pgn.h>

#include <QAction>
#include <QUrl>
#include <QDesktopServices>
#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDate>
#include <QDir>
#include <QDockWidget>
#include <QPixmap>
#include <QHBoxLayout>
#include <QDialogButtonBox>
#include <QDialog>
#include <QHash>
#include <QEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMenuBar>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QStyle>
#include <QToolButton>
#include <QRegularExpression>
#include <QFile>
#include <QSettings>
#include <QStatusBar>
#include <QTimer>
#include <QToolBar>
#include <QVBoxLayout>

#include <algorithm>
#include <functional>

using namespace mbc;

// Version of the dock layout; a saved layout of another version is ignored.
static const int kLayoutVersion = 2;
using mbc::engine::SearchInfo;
using mbc::engine::UciEngine;

MainWindow::MainWindow(LanguageManager *languages, QWidget *parent)
    : QMainWindow(parent)
    , m_languages(languages)
    , m_gm(new GameManager(languages, this))
    , m_game(m_gm->game())
{
    startup::progress(15, tr("Window, board styles and piece sets"));
    createUi();

    // The window shows what the game manager does.
    connect(m_gm, &GameManager::changed, this, &MainWindow::updateView);
    connect(m_gm, &GameManager::engineOutputChanged, this, [this](const QString &line) {
        if (!m_analysis->isRunning())
            m_engineOutput->setText(line);
    });
    connect(m_gm, &GameManager::movePlayed, m_sound, &SoundPlayer::playMove);
    connect(m_gm, &GameManager::gameEnded, this, &MainWindow::onGameEnded);
    connect(m_gm, &GameManager::playerNamesChanged, m_clockWidget, &ClockWidget::setNames);
    connect(m_gm, &GameManager::engineError, this, &MainWindow::onEngineError);
    connect(m_gm, &GameManager::statusMessage, this, &MainWindow::showStatus);
    connect(m_gm, &GameManager::chessbookProgress, this, [this](const QString &text) {
        m_chessbookLabel->setText(text);
        m_chessbookLabel->setVisible(!text.isEmpty());
    });

    // Start with a human-vs-human game without clock; "New game" offers the saved settings.
    startup::progress(45, tr("Engines, opening book and chessbooks"));
    GameSettings initial = GameSettings::load();
    initial.mode = GameSettings::HumanVsHuman;
    initial.timeControl = TimeControl();
    startGame(initial, Position::startPosition());
    m_gm->setWaitingForGame(true); // no moves until the user starts or opens a game
    updateView();

    startup::progress(65, tr("Texts and window layout"));
    retranslateUi();
    restoreWindowState();

#ifdef MBCHESS_HAVE_DGT
    m_dgt = new dgt::Board(this);
    m_dgtScanner = new dgt::Scanner(this);
    connect(m_dgt, &dgt::Board::connected, this, [this] {
        showStatus(tr("DGT board connected on %1.").arg(m_dgt->portName()), 5000);
        updateDgtStatus();
    });
    connect(m_dgt, &dgt::Board::boardChanged, this, &MainWindow::onDgtBoardChanged);
    connect(m_dgt, &dgt::Board::connectionLost, this, [this](const QString &reason) {
        showStatus(reason, 8000);
        m_dgtStatus->setText(tr("not connected"));
        m_dgtStatus->setToolTip(reason + QLatin1Char('\n')
                                + tr("mbChess connects again as soon as the board answers."));
        updateDgtStatus(); // icon and menu: no board any more
    });
    connect(m_dgtScanner, &dgt::Scanner::probing, this, [this](const QString &port) {
        if (!m_dgtQuiet && dgtWanted())
            m_dgtStatus->setText(tr("checking %1 ...").arg(port));
    });
    connect(m_dgtScanner, &dgt::Scanner::portProbed, this, [this](const dgt::ProbeResult &r) {
        if (r.found && m_dgtFoundPort.isEmpty())
            m_dgtFoundPort = r.port;
    });
    connect(m_dgtScanner, &dgt::Scanner::finished, this, &MainWindow::dgtSearchFinished);
    // plugging in / switching on: the watcher looks at the ports every 2 s
    m_dgtWatcher = new QTimer(this);
    m_dgtWatcher->setInterval(2000);
    connect(m_dgtWatcher, &QTimer::timeout, this, &MainWindow::dgtWatch);
    m_dgtKnownPorts = dgtPortNames();
    updateDgtIcon();
    m_dgtWatcher->start();
#endif
}

MainWindow::~MainWindow() = default; // the game manager stops the engines

bool MainWindow::confirmQuit()
{
    const bool benchmark = m_benchmark && m_benchmark->isRunning();
    const bool game = m_gm->gameInProgress();
    const bool unsaved = m_gm->hasUnsavedChanges();
    if (!benchmark && !game && !unsaved)
        return true;
    QString text;
    if (game)
        text += tr("<p>A game is running. If you quit now, it is ended (save it first with File > Save game "
                   "as PGN if you want to keep it).</p>");
    else if (unsaved)
        text += tr("<p>Your game may contain unsaved data: moves, variations or comments that are not "
                   "saved in a PGN file yet. After quitting they cannot be saved any more.</p>");
    if (benchmark)
        text += tr("<p>The engine benchmark is still running (game %1 of %2). It is <b>cancelled</b> now; "
                   "the results are lost unless you save the games in the results window first.</p>")
                    .arg(m_benchmark->currentIndex() + 1)
                    .arg(m_benchmark->totalGames());
    text += tr("<p>Quit mbChess anyway?</p>");
    QMessageBox box(QMessageBox::Question, tr("Quit mbChess"), text, QMessageBox::NoButton, this);
    QPushButton *save = unsaved ? box.addButton(tr("&Save game..."), QMessageBox::ActionRole) : nullptr;
    QPushButton *quit = box.addButton(tr("&Quit"), QMessageBox::AcceptRole);
    QPushButton *stay = box.addButton(tr("&Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(save ? save : stay);
    box.setEscapeButton(stay);
    box.exec();
    if (save && box.clickedButton() == save) {
        savePgn();
        if (m_gm->hasUnsavedChanges())
            return false; // not saved (dialog cancelled or error): stay
        return !benchmark || confirmQuit(); // a running benchmark still asks
    }
    return box.clickedButton() == quit;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!confirmQuit()) {
        event->ignore();
        return;
    }
    saveWindowState();
    m_gm->stopPlaying();
    if (m_benchmark && m_benchmark->isRunning())
        m_benchmark->cancel();
    if (m_benchmarkWindow)
        m_benchmarkWindow->close();
#ifdef Q_OS_WIN
    htmlhelp::closeAll();
#endif
    QMainWindow::closeEvent(event);
}

// ---------------------------------------------------------------------------
// UI

void MainWindow::createUi()
{
    m_fileMenu = menuBar()->addMenu(QString());
    m_openAction = m_fileMenu->addAction(QString(), this, &MainWindow::openPgn);
    m_openAction->setShortcut(QKeySequence::Open);
    m_saveAction = m_fileMenu->addAction(QString(), this, &MainWindow::savePgn);
    m_saveAction->setShortcut(QKeySequence::Save);
    m_fileMenu->addSeparator();
    m_prevGameAction = m_fileMenu->addAction(QString(), this, [this] { stepDatabaseGame(-1); });
    m_prevGameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp));
    m_prevGameAction->setEnabled(false);
    m_nextGameAction = m_fileMenu->addAction(QString(), this, [this] { stepDatabaseGame(1); });
    m_nextGameAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown));
    m_nextGameAction->setEnabled(false);
    m_fileMenu->addSeparator();
    m_setFenAction = m_fileMenu->addAction(QString(), this, &MainWindow::setFenDialog);
    m_copyFenAction = m_fileMenu->addAction(QString(), this, &MainWindow::copyFen);
    m_fileMenu->addSeparator();
    m_benchmarkMenu = m_fileMenu->addMenu(QString());
    m_benchmarkStartAction = m_benchmarkMenu->addAction(QString(), this, &MainWindow::startBenchmark);
    m_benchmarkCancelAction = m_benchmarkMenu->addAction(QString(), this, &MainWindow::cancelBenchmark);
    m_benchmarkFollowAction = m_benchmarkMenu->addAction(QString());
    m_benchmarkFollowAction->setCheckable(true);
    m_benchmarkFollowAction->setChecked(QSettings().value(QStringLiteral("benchmark/follow"), true).toBool());
    connect(m_benchmarkFollowAction, &QAction::toggled, this, &MainWindow::setFollowBenchmark);
    m_benchmarkMenu->addSeparator();
    m_benchmarkResultsAction = m_benchmarkMenu->addAction(QString(), this, &MainWindow::showBenchmarkResults);
    m_fileMenu->addSeparator();
    m_quitAction = m_fileMenu->addAction(QString(), this, &QWidget::close);
    m_quitAction->setShortcut(QKeySequence::Quit);

    // Play: new game, quick starts against opponents of fixed strength or style
    m_playMenu = menuBar()->addMenu(QString());
    m_newGameAction = m_playMenu->addAction(QString(), this, [this] { newGameDialog(); });
    m_newGameAction->setShortcut(QKeySequence::New);
    m_playMenu->addSeparator();
    m_beginnerMenu = m_playMenu->addMenu(QString());
    m_computerMenu = m_playMenu->addMenu(QString());
    m_characterMenu = m_playMenu->addMenu(QString());
    const QList<QPair<QMenu *, opponents::Group>> opponentMenus = {
        { m_beginnerMenu, opponents::Group::Beginner },
        { m_computerMenu, opponents::Group::Computer },
        { m_characterMenu, opponents::Group::Character },
    };
    for (const auto &entry : opponentMenus) {
        QMenu *menu = entry.first;
        const int group = int(entry.second);
        menu->setToolTipsVisible(true);
        connect(menu, &QMenu::aboutToShow, this, [this, menu, group] { fillOpponentMenu(menu, group); });
        fillOpponentMenu(menu, group);
    }
    m_playMenu->addSeparator();
    m_engineVsEngineAction = m_playMenu->addAction(QString(), this, [this] { newGameDialog(true); });

    m_gameMenu = menuBar()->addMenu(QString());
    m_takeBackAction = m_gameMenu->addAction(QString(), m_gm, &GameManager::takeBack);
    m_takeBackAction->setShortcut(QKeySequence::Undo);
    m_moveNowAction = m_gameMenu->addAction(QString(), m_gm, &GameManager::moveNow);
    m_moveNowAction->setShortcut(QKeySequence(Qt::Key_Space));
    m_gameMenu->addSeparator();
    m_resignAction = m_gameMenu->addAction(QString(), m_gm, &GameManager::resign);
    m_gameMenu->addSeparator();
    m_startAction = m_gameMenu->addAction(QString(), this, &MainWindow::goToStart);
    m_startAction->setShortcut(QKeySequence(Qt::Key_Home));
    m_backAction = m_gameMenu->addAction(QString(), this, &MainWindow::goBack);
    m_backAction->setShortcut(QKeySequence(Qt::Key_Left));
    m_forwardAction = m_gameMenu->addAction(QString(), this, &MainWindow::goForward);
    m_forwardAction->setShortcut(QKeySequence(Qt::Key_Right));
    m_endAction = m_gameMenu->addAction(QString(), this, &MainWindow::goToEnd);
    m_endAction->setShortcut(QKeySequence(Qt::Key_End));
    m_gameMenu->addSeparator();
    m_setupPositionAction = m_gameMenu->addAction(QString(), this, &MainWindow::setupPosition);
    m_setupPositionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));

    m_engineMenu = menuBar()->addMenu(QString());
    m_analysis = new EngineAnalysis(this);
    connect(m_analysis, &EngineAnalysis::updated, this, &MainWindow::onAnalysisUpdated);
    connect(m_analysis, &EngineAnalysis::errorOccurred, this, [this](const QString &message) {
        m_analysisAction->setChecked(false);
        QMessageBox::warning(this, tr("Engine analysis"), message);
    });
    m_analysisAction = m_engineMenu->addAction(QString());
    m_analysisAction->setCheckable(true);
    m_analysisAction->setShortcut(QKeySequence(Qt::ALT | Qt::Key_F2));
    m_analysisAction->setIconVisibleInMenu(false); // the check mark shows the state
    connect(m_analysisAction, &QAction::toggled, this, &MainWindow::setAnalysis);
    m_analysisEngineMenu = m_engineMenu->addMenu(QString());
    m_analysisEngineGroup = new QActionGroup(this);
    connect(m_analysisEngineMenu, &QMenu::aboutToShow, this, &MainWindow::fillAnalysisEngineMenu);
    m_analysisLinesMenu = m_engineMenu->addMenu(QString());
    m_analysisLinesGroup = new QActionGroup(this);
    {
        const int lines = qBound(1, QSettings().value(QStringLiteral("analysis/lines"), 3).toInt(), 5);
        m_analysis->setMultiPv(lines);
        for (int n = 1; n <= 5; ++n) {
            QAction *a = m_analysisLinesMenu->addAction(QString::number(n));
            a->setCheckable(true);
            a->setChecked(n == lines);
            a->setData(n);
            m_analysisLinesGroup->addAction(a);
            connect(a, &QAction::triggered, this, [this, n] {
                QSettings().setValue(QStringLiteral("analysis/lines"), n);
                m_analysis->setMultiPv(n);
            });
        }
    }
    m_mateSearchAction = m_engineMenu->addAction(QString(), this, &MainWindow::mateSearch);
    m_mateSearchAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_M));
    m_engineMenu->addSeparator();
    m_manageEnginesAction = m_engineMenu->addAction(QString(), this, &MainWindow::manageEngines);
    m_tablebaseAction = m_engineMenu->addAction(QString(), this, &MainWindow::tablebaseDialog);

    m_boardMenu = menuBar()->addMenu(QString());
    m_flipAction = m_boardMenu->addAction(QString(), this, &MainWindow::flipBoard);
    m_flipAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_R));
    m_coordinatesAction = m_boardMenu->addAction(QString());
    m_coordinatesAction->setCheckable(true);
    m_coordinatesAction->setChecked(QSettings().value(QStringLiteral("board/coordinates"), true).toBool());
    connect(m_coordinatesAction, &QAction::toggled, this,
            [](bool on) { QSettings().setValue(QStringLiteral("board/coordinates"), on); });
    m_boardMenu->addSeparator();
    m_boardThemeMenu = m_boardMenu->addMenu(QString());
    m_pieceSetMenu = m_boardMenu->addMenu(QString());
    m_boardMenu->addSeparator();
    m_effectsAction = m_boardMenu->addAction(QString());
    m_speechAction = m_boardMenu->addAction(QString());
    m_voiceMenu = m_boardMenu->addMenu(QString());
    m_boardMenu->addSeparator();
    m_dgtAction = m_boardMenu->addAction(QString());
    m_dgtAction->setCheckable(true);
    m_dgtAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    m_dgtAction->setIconVisibleInMenu(false); // the check mark shows the state
    connect(m_dgtAction, &QAction::triggered, this, &MainWindow::setDgt);
    m_dgtTakeAction = m_boardMenu->addAction(QString(), this, &MainWindow::dgtTakePosition);
    m_dgtSettingsAction = m_boardMenu->addAction(QString(), this, &MainWindow::dgtDialog);

    m_viewMenu = menuBar()->addMenu(QString());   // filled below, once the docks exist

    m_languageMenu = menuBar()->addMenu(QString());
    m_languageGroup = new QActionGroup(this);
    // Language names are shown in their own language and are not translated.
    m_germanAction = m_languageMenu->addAction(QStringLiteral("Deutsch"));
    m_englishAction = m_languageMenu->addAction(QStringLiteral("English"));
    for (QAction *a : { m_germanAction, m_englishAction }) {
        a->setCheckable(true);
        m_languageGroup->addAction(a);
    }
    connect(m_germanAction, &QAction::triggered, this, [this] { m_languages->setLanguage(QStringLiteral("de")); });
    connect(m_englishAction, &QAction::triggered, this, [this] { m_languages->setLanguage(QStringLiteral("en")); });

    m_helpMenu = menuBar()->addMenu(QString());
    m_manualAction = m_helpMenu->addAction(QString(), this, &MainWindow::showManual);
    m_manualAction->setShortcut(QKeySequence::HelpContents); // F1
    m_helpMenu->addSeparator();
    m_aboutAction = m_helpMenu->addAction(QString(), this, &MainWindow::showAbout);
    m_aboutQtAction = m_helpMenu->addAction(QString(), qApp, &QApplication::aboutQt);

    // --- Central widget: the board with status and FEN below ---
    auto *central = new QWidget(this);
    auto *centralLayout = new QVBoxLayout(central);
    m_board = new BoardWidget(central);
    connect(m_board, &BoardWidget::moveMade, this, &MainWindow::onBoardMove);
    connect(m_coordinatesAction, &QAction::toggled, m_board, &BoardWidget::setShowCoordinates);
    m_board->setShowCoordinates(m_coordinatesAction->isChecked());
    createThemeMenus();
    // status bar: "Status: ... | Chessbook: ... | Opening: ... | Benchmark ... | DGT: ..."
    m_statusField = new StatusField(this);
    m_statusField->setDivider(false);
    statusBar()->addWidget(m_statusField, 1);
    m_statusTimer = new QTimer(this);
    m_statusTimer->setSingleShot(true);
    connect(m_statusTimer, &QTimer::timeout, this, [this] { showStatus(QString()); });
    m_chessbookLabel = new StatusField(this);
    m_chessbookLabel->hide();
    statusBar()->addPermanentWidget(m_chessbookLabel);
    m_openingLabel = new StatusField(this);
    m_openingLabel->setMaximumTextWidth(420);
    statusBar()->addPermanentWidget(m_openingLabel);

    // benchmark: "Benchmark: [A vs. B  3/56]" with a cancel button, only while it runs
    m_benchmark = new BenchmarkRunner(this);
    m_benchmarkStatus = new QWidget(this);
    auto *benchLayout = new QHBoxLayout(m_benchmarkStatus);
    benchLayout->setContentsMargins(0, 0, 0, 0);
    auto *benchDivider = new QFrame(m_benchmarkStatus);
    benchDivider->setFrameShape(QFrame::VLine);
    benchDivider->setFrameShadow(QFrame::Sunken);
    benchLayout->addWidget(benchDivider);
    m_benchmarkLabel = new QLabel(m_benchmarkStatus);
    m_benchmarkProgress = new QProgressBar(m_benchmarkStatus);
    m_benchmarkProgress->setMinimumWidth(440);
    m_benchmarkProgress->setTextVisible(true);
    m_benchmarkCancelButton = new QToolButton(m_benchmarkStatus);
    m_benchmarkCancelButton->setAutoRaise(true);
    m_benchmarkCancelButton->setIcon(style()->standardIcon(QStyle::SP_DialogCancelButton));
    connect(m_benchmarkCancelButton, &QToolButton::clicked, this, &MainWindow::cancelBenchmark);
    benchLayout->addWidget(m_benchmarkLabel);
    benchLayout->addWidget(m_benchmarkProgress);
    benchLayout->addWidget(m_benchmarkCancelButton);
    statusBar()->addPermanentWidget(m_benchmarkStatus);
    m_dgtStatus = new StatusField(this);
    m_dgtStatus->setMaximumTextWidth(360);
    m_dgtStatus->hide();
    statusBar()->addPermanentWidget(m_dgtStatus);
    m_benchmarkStatus->hide();
    connect(m_benchmark, &BenchmarkRunner::gameStarted, this,
            [this](int index, int total, const QString &, const QString &) {
                m_benchmarkProgress->setRange(0, total);
                m_benchmarkProgress->setValue(index);
                updateBenchmarkStatus();
                m_benchmarkStatus->show();
                m_benchmarkTimer->start();
            });
    connect(m_benchmark, &BenchmarkRunner::positionChanged, this, &MainWindow::syncBenchmarkGame);
    connect(m_benchmark, &BenchmarkRunner::gameFinished, this, &MainWindow::benchmarkGameFinished);
    connect(m_benchmark, &BenchmarkRunner::searchInfo, this, [this](int side, const SearchInfo &info) {
        if (!m_watching || !m_benchmark->isInGame() || (!info.hasScore && info.pv.isEmpty()))
            return;
        const Game &g = m_benchmark->currentGame();
        Evaluation eval;
        m_gm->setEngineLine(m_gm->engineInfoText(g.nodePosition(g.mainLine().isEmpty() ? Game::RootNode : g.mainLine().last()),
                                      side == 0 ? White : Black, info, &eval));
        m_engineOutput->setText(m_gm->engineLine());
    });
    m_benchmarkTimer = new QTimer(this);
    m_benchmarkTimer->setInterval(15000);
    connect(m_benchmarkTimer, &QTimer::timeout, this, &MainWindow::updateBenchmarkStatus);
    connect(m_benchmark, &BenchmarkRunner::finished, this, [this](bool cancelled) {
        m_benchmarkTimer->stop();
        m_benchmarkStatus->hide();
        stopWatching();
        updateBenchmarkActions();
        // The results window opens only on request (File > Benchmarks > Show results).
        showStatus(cancelled ? tr("Benchmark cancelled.")
                                           : tr("Benchmark finished: the results are in File > Benchmarks > Show results."),
                                 15000);
        if (!cancelled) {
            QApplication::alert(this);
            if (m_benchmark->settings().showResultsAtEnd) // optional, only after a complete run
                showBenchmarkResults();
        }
    });
    m_status = new QLabel(central);
    m_status->setWordWrap(true);
    connect(m_status, &QLabel::linkActivated, this, [this](const QString &link) {
        if (link == QLatin1String("new"))
            newGameDialog();
    });
    m_fen = new QLabel(central);
    m_fen->setTextInteractionFlags(Qt::TextSelectableByMouse);
    centralLayout->addWidget(m_board, 1);
    centralLayout->addWidget(m_status);
    centralLayout->addWidget(m_fen);
    setCentralWidget(central);

    // --- Dock windows ---
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks | QMainWindow::AllowTabbedDocks);
    auto makeDock = [this](const char *objectName, QWidget *content) {
        auto *dock = new QDockWidget(this);
        dock->setObjectName(QLatin1String(objectName)); // needed for saveState()
        dock->setWidget(content);
        dock->setAllowedAreas(Qt::AllDockWidgetAreas);
        return dock;
    };

    m_clockWidget = new ClockWidget(m_gm->clock(), this);
    m_clockDock = makeDock("clockDock", m_clockWidget);

    auto *enginePanel = new QWidget(this);
    auto *engineLayout = new QVBoxLayout(enginePanel);
    m_engineTitle = new QLabel(enginePanel);
    m_engineTitle->setWordWrap(true);
    QFont bold = m_engineTitle->font();
    bold.setBold(true);
    m_engineTitle->setFont(bold);
    m_engineOutput = new QLabel(enginePanel);
    m_engineOutput->setWordWrap(true);
    m_engineOutput->setMinimumHeight(m_engineOutput->fontMetrics().lineSpacing() * 3);
    m_engineOutput->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_engineOutput->setTextInteractionFlags(Qt::TextSelectableByMouse);
    engineLayout->addWidget(m_engineTitle);
    engineLayout->addWidget(m_engineOutput, 1);
    m_engineDock = makeDock("engineDock", enginePanel);

    auto *notationPanel = new QWidget(this);
    auto *notationLayout = new QVBoxLayout(notationPanel);
    m_notation = new NotationWidget(notationPanel);
    m_notation->setGame(&m_game);
    connect(m_notation, &NotationWidget::nodeClicked, this, &MainWindow::goToNode);
    connect(m_notation, &NotationWidget::promoteRequested, m_gm, &GameManager::promoteVariation);
    connect(m_notation, &NotationWidget::deleteRequested, m_gm, &GameManager::deleteVariation);
    connect(m_notation, &NotationWidget::commentRequested, this, &MainWindow::editComment);
    m_input = new QLineEdit(notationPanel);
    m_moveButton = new QPushButton(notationPanel);
    m_input->installEventFilter(this);
    connect(m_input, &QLineEdit::returnPressed, this, &MainWindow::enterMove);
    connect(m_moveButton, &QPushButton::clicked, this, &MainWindow::enterMove);
    auto *inputRow = new QHBoxLayout;
    inputRow->addWidget(m_input, 1);
    inputRow->addWidget(m_moveButton);
    m_inputLabel = new QLabel(notationPanel);
    m_inputLabel->setWordWrap(true);
    notationLayout->addWidget(m_notation, 1);
    notationLayout->addLayout(inputRow);
    notationLayout->addWidget(m_inputLabel);
    m_notationDock = makeDock("notationDock", notationPanel);

    m_graph = new EvalGraphWidget(this);
    m_graph->setGame(&m_game);
    connect(m_graph, &EvalGraphWidget::nodeClicked, this, &MainWindow::goToNode);
    m_graphDock = makeDock("evalGraphDock", m_graph);

    m_gameList = new GameListWidget(this);
    connect(m_gameList, &GameListWidget::gamesLoaded, this, &MainWindow::databaseLoaded);
    connect(m_gameList, &GameListWidget::loadFailed, this, [this](const QString &error) {
        if (!error.isEmpty())
            QMessageBox::warning(this, tr("Open game"), tr("The file could not be opened:\n%1").arg(error));
        updateDatabaseActions();
    });
    connect(m_gameList, &GameListWidget::gameActivated, this, [this](int index) {
        if (m_benchmark->isRunning()) {
            showStatus(tr("Not possible while the benchmark runs."), 5000);
            return;
        }
        if (confirmEndGame(tr("Open game"), tr("Opening another game ends this game.")))
            loadDatabaseGame(index);
    });
    m_gameListDock = makeDock("gameListDock", m_gameList);

    // Default layout: clock, engine and notation right of the board; below board and
    // right column a strip across the whole width (evaluation graph, later more).
    // The separators between them are splitters: dragging the vertical one scales the board.
    setCorner(Qt::BottomLeftCorner, Qt::BottomDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::BottomDockWidgetArea);
    addDockWidget(Qt::RightDockWidgetArea, m_clockDock);
    splitDockWidget(m_clockDock, m_engineDock, Qt::Vertical);
    splitDockWidget(m_engineDock, m_notationDock, Qt::Vertical);
    addDockWidget(Qt::BottomDockWidgetArea, m_graphDock);
    resizeDocks({ m_graphDock }, { 140 }, Qt::Vertical);
    // The game list shares the bottom strip with the graph (tabs); shown when a file
    // with several games is opened.
    tabifyDockWidget(m_graphDock, m_gameListDock);
    m_gameListDock->hide();
    resizeDocks({ m_notationDock }, { 420 }, Qt::Horizontal);
    setStyleSheet(QStringLiteral(
        "QMainWindow::separator { background: palette(mid); width: 5px; height: 5px; }"
        "QMainWindow::separator:hover { background: palette(highlight); }"));

    m_saveStateTimer = new QTimer(this);
    m_saveStateTimer->setSingleShot(true);
    m_saveStateTimer->setInterval(1000);
    connect(m_saveStateTimer, &QTimer::timeout, this, &MainWindow::saveWindowState);
    for (QDockWidget *dock : { m_clockDock, m_engineDock, m_notationDock, m_graphDock, m_gameListDock }) {
        connect(dock, &QDockWidget::dockLocationChanged, m_saveStateTimer, qOverload<>(&QTimer::start));
        connect(dock, &QDockWidget::visibilityChanged, m_saveStateTimer, qOverload<>(&QTimer::start));
        connect(dock, &QDockWidget::topLevelChanged, m_saveStateTimer, qOverload<>(&QTimer::start));
    }
    createToolBar();

    m_viewMenu->addAction(m_toolBar->toggleViewAction());
    for (QDockWidget *dock : { m_clockDock, m_engineDock, m_notationDock, m_graphDock, m_gameListDock })
        m_viewMenu->addAction(dock->toggleViewAction());
    m_viewMenu->addSeparator();
    m_guiThemeMenu = m_viewMenu->addMenu(QString());
    m_guiThemeGroup = new QActionGroup(this);
    for (const QString &id : guitheme::ids()) {
        QAction *a = m_guiThemeMenu->addAction(QString());
        a->setData(id);
        a->setCheckable(true);
        a->setChecked(id == guitheme::current());
        m_guiThemeGroup->addAction(a);
        if (id == guitheme::defaultId())
            m_guiThemeMenu->addSeparator();
        connect(a, &QAction::triggered, this, [this, id] {
            guitheme::apply(id);
            QSettings().setValue(QStringLiteral("ui/guiTheme"), id);
            updateToolBarIcons();
        });
    }
    m_resetLayoutAction = m_viewMenu->addAction(QString(), this, [this] {
        for (QDockWidget *dock : { m_clockDock, m_engineDock, m_notationDock, m_graphDock, m_gameListDock })
            dock->setFloating(false);
        restoreState(m_defaultLayout, kLayoutVersion);
        m_toolBar->show();
    });
    m_viewMenu->addSeparator();
    m_splashAction = m_viewMenu->addAction(QString());
    m_splashAction->setCheckable(true);
    m_splashAction->setChecked(SplashScreen::enabled());
    connect(m_splashAction, &QAction::toggled, this, [](bool on) { SplashScreen::setEnabled(on); });
}

void MainWindow::restoreWindowState()
{
    m_defaultLayout = saveState(kLayoutVersion);
    QSettings s;
    const QByteArray geometry = s.value(QStringLiteral("window/geometry")).toByteArray();
    if (geometry.isEmpty() || !restoreGeometry(geometry))
        resize(1180, 800);
    restoreState(s.value(QStringLiteral("window/state")).toByteArray(), kLayoutVersion);
    m_windowRestored = true;
}

void MainWindow::saveWindowState()
{
    if (!m_windowRestored)
        return;
    QSettings s;
    s.setValue(QStringLiteral("window/geometry"), saveGeometry());
    s.setValue(QStringLiteral("window/state"), saveState(kLayoutVersion));
}

void MainWindow::moveEvent(QMoveEvent *event)
{
    QMainWindow::moveEvent(event);
    if (m_saveStateTimer)
        m_saveStateTimer->start(); // saved a second after the last change, not only at the end
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (m_saveStateTimer)
        m_saveStateTimer->start();
}

void MainWindow::createToolBar()
{
    m_toolBar = addToolBar(QString());
    m_toolBar->setObjectName(QStringLiteral("mainToolBar"));
    m_toolBar->setIconSize(QSize(24, 24));
    m_toolBar->setMovable(true);
    connect(m_toolBar, &QToolBar::visibilityChanged, m_saveStateTimer, qOverload<>(&QTimer::start));
    updateToolBarIcons();
    m_toolBar->addActions({ m_newGameAction, m_openAction, m_saveAction });
    m_toolBar->addSeparator();
    m_toolBar->addActions({ m_takeBackAction, m_moveNowAction, m_resignAction });
    m_toolBar->addSeparator();
    m_toolBar->addActions({ m_startAction, m_backAction, m_forwardAction, m_endAction });
    m_toolBar->addSeparator();
    m_toolBar->addActions({ m_flipAction, m_analysisAction, m_setupPositionAction, m_mateSearchAction });
    if (SoundPlayer::isAvailable())
        m_toolBar->addActions({ m_effectsAction, m_speechAction });
    m_toolBar->addAction(m_dgtAction);
}

void MainWindow::updateToolBarIcons()
{
    // light icons on dark window themes
    auto icon = [](const char *name) { return guitheme::toolbarIcon(QLatin1String(name)); };
    m_newGameAction->setIcon(icon("new"));
    m_openAction->setIcon(icon("open"));
    m_saveAction->setIcon(icon("save"));
    m_takeBackAction->setIcon(icon("takeback"));
    m_moveNowAction->setIcon(icon("movenow"));
    m_resignAction->setIcon(icon("resign"));
    m_startAction->setIcon(icon("first"));
    m_backAction->setIcon(icon("back"));
    m_forwardAction->setIcon(icon("forward"));
    m_endAction->setIcon(icon("last"));
    m_flipAction->setIcon(icon("flip"));
    m_analysisAction->setIcon(icon("analysis"));
    m_setupPositionAction->setIcon(icon("setup"));
    m_mateSearchAction->setIcon(icon("mate"));
    updateSoundIcons();
    updateDgtIcon();
}

void MainWindow::updateSoundIcons()
{
    auto icon = [](const char *name) { return guitheme::toolbarIcon(QLatin1String(name)); };
    const bool effects = m_effectsAction->isChecked();
    const bool speech = m_speechAction->isChecked();
    m_effectsAction->setIcon(icon(effects ? "sound" : "sound-off"));
    m_speechAction->setIcon(icon(speech ? "speech" : "speech-off"));
    // In the menu a check mark shows the state (an icon would hide it).
    m_effectsAction->setIconVisibleInMenu(false);
    m_speechAction->setIconVisibleInMenu(false);
    m_effectsAction->setToolTip(effects ? tr("Move sounds are on (click to switch off)")
                                        : tr("Move sounds are off (click to switch on)"));
    m_speechAction->setToolTip(speech ? tr("Moves are announced (click to switch off)")
                                      : tr("Moves are not announced (click to switch on)"));
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("mbChess"));
    m_fileMenu->setTitle(tr("&File"));
    m_playMenu->setTitle(tr("&Play"));
    m_newGameAction->setText(tr("&New game..."));
    m_beginnerMenu->setTitle(tr("&Beginners"));
    m_computerMenu->setTitle(tr("Against the &computer"));
    m_characterMenu->setTitle(tr("Opponents with c&haracter"));
    m_engineVsEngineAction->setText(tr("&Engine against engine..."));
    for (QMenu *menu : { m_beginnerMenu, m_computerMenu, m_characterMenu })
        menu->clear(); // refilled in the new language when opened
    m_openAction->setText(tr("&Open game (PGN)..."));
    m_saveAction->setText(tr("&Save game as PGN..."));
    m_setFenAction->setText(tr("Set position from &FEN..."));
    m_copyFenAction->setText(tr("&Copy FEN"));
    m_quitAction->setText(tr("&Quit"));
    m_benchmarkMenu->setTitle(tr("&Benchmarks"));
    m_benchmarkStartAction->setText(tr("&Engine tournament..."));
    m_benchmarkCancelAction->setText(tr("&Cancel benchmark"));
    m_benchmarkResultsAction->setText(tr("Show &results"));
    m_benchmarkFollowAction->setText(tr("&Follow the games on the board"));
    m_benchmarkLabel->setText(tr("Benchmark:"));
    m_statusField->setCaption(tr("Status:"));
    if (m_statusMessage.isEmpty())
        m_statusField->setText(tr("Ready"));
    m_chessbookLabel->setCaption(tr("Chessbook:"));
    m_openingLabel->setCaption(tr("Opening:"));
    m_dgtStatus->setCaption(tr("DGT:"));
    m_benchmarkCancelButton->setToolTip(tr("Cancel the benchmark"));
    updateBenchmarkActions();
    m_gameMenu->setTitle(tr("&Game"));
    m_engineMenu->setTitle(tr("&Engines"));
    m_manageEnginesAction->setText(tr("Manage e&ngines..."));
    m_tablebaseAction->setText(tr("&Endgame tablebases..."));
    m_analysisAction->setText(tr("Engine &analysis"));
    m_analysisAction->setToolTip(tr("Engine analysis: an engine searches the position on the board (Alt+F2)"));
    m_analysisEngineMenu->setTitle(tr("Analysis en&gine"));
    m_analysisLinesMenu->setTitle(tr("Analysis &lines"));
    m_mateSearchAction->setText(tr("&Mate search..."));
    m_mateSearchAction->setToolTip(tr("Mate search: an engine looks for a mate in the position on the board (Ctrl+M)"));
    m_setupPositionAction->setText(tr("Set &up position..."));
    m_setupPositionAction->setToolTip(tr("Set up a position with the board editor (Ctrl+E)"));
    m_takeBackAction->setText(tr("&Take back move"));
    m_moveNowAction->setText(tr("&Move now!"));
    m_resignAction->setText(tr("&Resign"));
    m_boardMenu->setTitle(tr("&Board"));
    m_flipAction->setText(tr("&Flip board"));
    m_coordinatesAction->setText(tr("Show &coordinates"));
    retranslateThemeMenus();
    m_startAction->setText(tr("Go to the &start"));
    m_backAction->setText(tr("Move &back"));
    m_forwardAction->setText(tr("Move &forward"));
    m_endAction->setText(tr("Go to the &end"));
    m_viewMenu->setTitle(tr("&View"));
    m_clockDock->setWindowTitle(tr("Clock"));
    m_engineDock->setWindowTitle(tr("Engine"));
    m_notationDock->setWindowTitle(tr("Notation"));
    m_graphDock->setWindowTitle(tr("Evaluation"));
    m_gameListDock->setWindowTitle(tr("Games"));
    m_prevGameAction->setText(tr("P&revious game"));
    m_nextGameAction->setText(tr("Ne&xt game"));
    m_resetLayoutAction->setText(tr("&Reset window layout"));
    m_guiThemeMenu->setTitle(tr("Window &theme"));
    m_splashAction->setText(tr("Show the &start screen"));
    m_splashAction->setToolTip(tr("The start screen with the progress of loading, when mbChess starts"));
    for (QAction *a : m_guiThemeGroup->actions()) {
        const QString id = a->data().toString();
        a->setText(id == QLatin1String("dark") ? tr("Dark design") : tr("System"));
    }
    m_toolBar->setWindowTitle(tr("Toolbar"));
    m_languageMenu->setTitle(tr("&Language"));
    m_helpMenu->setTitle(tr("&Help"));
    m_aboutAction->setText(tr("&About mbChess"));
    m_manualAction->setText(tr("&Manual"));
    m_aboutQtAction->setText(tr("About &Qt"));
    m_input->setPlaceholderText(tr("e.g. e4, Nf3, O-O or g1f3"));
    m_inputLabel->setText(tr("Move with the mouse (click the piece, then the target square, or drag it), "
                             "or type a move in algebraic or UCI notation."));
    m_moveButton->setText(tr("&Move"));

    const bool german = m_languages->language() == QLatin1String("de");
    updateSpeechLanguage();
    m_germanAction->setChecked(german);
    m_englishAction->setChecked(!german);

    m_gm->updatePlayerNames();
    m_clockWidget->refresh();
    updateView();
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    // While the move input is empty, the arrow keys, Home and End move through the game
    // instead of the (non-existent) text.
    if (watched == m_input && event->type() == QEvent::ShortcutOverride && m_input->text().isEmpty()) {
        const auto *k = static_cast<QKeyEvent *>(event);
        if (k->modifiers() == Qt::NoModifier
            && (k->key() == Qt::Key_Left || k->key() == Qt::Key_Right || k->key() == Qt::Key_Home
                || k->key() == Qt::Key_End)) {
            event->ignore();
            return true;
        }
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::LanguageChange && m_fileMenu)
        retranslateUi();
    QMainWindow::changeEvent(event);
}

// ---------------------------------------------------------------------------
// Game flow

void MainWindow::fillOpponentMenu(QMenu *menu, int group)
{
    menu->clear();
    const QVector<opponents::Opponent> list = opponents::list(opponents::Group(group));
    const bool running = m_benchmark && m_benchmark->isRunning();
    for (int i = 0; i < list.size(); ++i) {
        const opponents::Opponent &o = list.at(i);
        //: menu Play > Beginners: "1  Novice (about 400 Elo)"
        QAction *a = menu->addAction(group == int(opponents::Group::Beginner)
                                         ? QStringLiteral("&%1  %2").arg(i + 1).arg(opponents::menuText(o))
                                         : opponents::menuText(o),
                                     this, [this, group, i] { startOpponent(group, i); });
        a->setToolTip(opponents::toolTip(o));
        a->setEnabled(!running && opponents::isAvailable(o));
    }
}

void MainWindow::startOpponent(int group, int index)
{
    const QVector<opponents::Opponent> list = opponents::list(opponents::Group(group));
    if (index < 0 || index >= list.size() || (m_benchmark && m_benchmark->isRunning()))
        return;
    const opponents::Opponent &o = list.at(index);
    if (!opponents::isAvailable(o)) {
        QMessageBox::warning(this, tr("New game"), opponents::toolTip(o));
        return;
    }
    if (!confirmEndGame(tr("New game"), tr("A new game ends this game.")))
        return;
    const GameSettings s = opponents::quickSettings(opponents::Group(group), o);
    startGame(s, Position::startPosition());
    showStatus(tr("New game against \"%1\".").arg(opponents::displayName(o)), 5000);
}

void MainWindow::newGameDialog(bool engineVsEngine)
{
    if (!confirmEndGame(tr("New game"), tr("A new game ends this game.")))
        return;
    NewGameDialog dlg(GameSettings::load(), this);
    if (engineVsEngine)
        dlg.setEngineVsEngine();
    // the position on the board, if it is not the initial one
    const Position board = m_game.position();
    const bool offer = !m_watching && board.toFen() != QLatin1String(Position::StartFen)
        && !board.legalMoves().empty();
    if (offer)
        dlg.offerBoardPosition(m_gm->isPositionSetUp());
    if (dlg.exec() != QDialog::Accepted)
        return;
    const GameSettings s = dlg.settings();
    s.save();
    startGame(s, dlg.fromBoardPosition() ? board : Position::startPosition());
}

void MainWindow::startGame(const GameSettings &settings, const Position &start)
{
    if (m_watching)
        m_benchmarkFollowAction->setChecked(false); // an own game ends following the benchmark
    m_board->setFlipped(settings.mode == GameSettings::HumanBlackVsEngine);
    m_board->setAutoQueen(settings.autoQueen);
    m_gm->startGame(settings, start);
}

void MainWindow::onGameEnded(Result result, const QString &reason)
{
    if (result == Result::Draw)
        m_sound->announce(m_game.status() == GameStatus::Stalemate ? QStringLiteral("stalemate")
                                                                    : QStringLiteral("draw"));
    showStatus(reason + QLatin1Char(' ') + resultToString(result), 8000);
}











void MainWindow::onEngineError(const QString &message)
{
    QMessageBox::warning(this, tr("Engine"), message);
    updateView();
}













void MainWindow::onBoardMove(Move move)
{
    if (!m_gm->humanMayMove()) {
        updateView();
        return;
    }
    m_gm->applyMove(move);
}


void MainWindow::goToNode(Game::NodeId id)
{
    if (!m_game.isValidNode(id) || id == m_game.currentNode())
        return;
    m_game.goToNode(id);
    updateView();
}

void MainWindow::goBack()
{
    if (m_game.goBack())
        updateView();
}

void MainWindow::goForward()
{
    if (m_game.goForward())
        updateView();
}

void MainWindow::goToStart()
{
    goToNode(Game::RootNode);
}

void MainWindow::goToEnd()
{
    m_game.goToLineEnd();
    updateView();
}




void MainWindow::editComment(Game::NodeId id)
{
    if (!m_game.isValidNode(id))
        return;
    bool ok = false;
    const QString move = localizeSan(m_game.nodeSan(id), m_languages->pieceLetters());
    const QString text = QInputDialog::getMultiLineText(this, tr("Comment"), tr("Comment after %1:").arg(move),
                                                        m_game.comment(id), &ok);
    if (!ok)
        return;
    m_game.setComment(id, text.trimmed());
    updateView();
}

// ---------------------------------------------------------------------------
// Themes and sounds

void MainWindow::createThemeMenus()
{
    QSettings s;
    m_sound = new SoundPlayer(this);

    // One action per theme, in submenus by group ("Lucas Chess/Felicia" -> two levels).
    auto addThemes = [this](QMenu *root, QActionGroup *group, const QStringList &ids, bool boards) {
        QHash<QString, QMenu *> submenus;
        std::function<QMenu *(const QString &)> menuFor = [&](const QString &path) -> QMenu * {
            if (path.isEmpty())
                return root;
            if (QMenu *m = submenus.value(path))
                return m;
            const int slash = path.lastIndexOf(QLatin1Char('/'));
            QMenu *parent = menuFor(slash < 0 ? QString() : path.left(slash));
            QMenu *m = parent->addMenu(path.mid(slash + 1));
            submenus.insert(path, m);
            return m;
        };
        // Ungrouped themes first, then the groups
        QList<themes::ThemeInfo> infos;
        for (const QString &id : ids)
            infos << (boards ? themes::boardInfo(id) : themes::pieceInfo(id));
        std::stable_sort(infos.begin(), infos.end(), [](const themes::ThemeInfo &x, const themes::ThemeInfo &y) {
            if (x.group.isEmpty() != y.group.isEmpty())
                return x.group.isEmpty();
            const int c = QString::compare(x.group, y.group, Qt::CaseInsensitive);
            return c != 0 ? c < 0 : QString::compare(x.name, y.name, Qt::CaseInsensitive) < 0;
        });
        for (const themes::ThemeInfo &info : infos) {
            if (!info.group.isEmpty() && submenus.isEmpty())
                root->addSeparator(); // between the program's own theme and the collections
            QMenu *menu = menuFor(info.group);
            QAction *a = menu->addAction(info.name);
            a->setData(info.id);
            a->setProperty("themeName", info.name);
            a->setCheckable(true);
            group->addAction(a);
            const QString id = info.id;
            connect(a, &QAction::triggered, this, [this, id, boards] {
                if (boards) {
                    m_board->setBoardTheme(id);
                    QSettings().setValue(QStringLiteral("board/theme"), m_board->boardTheme());
                } else {
                    m_board->setPieceSet(id);
                    QSettings().setValue(QStringLiteral("board/pieces"), m_board->pieceSet());
                }
            });
        }
    };
    m_boardThemeGroup = new QActionGroup(this);
    m_pieceSetGroup = new QActionGroup(this);
    addThemes(m_boardThemeMenu, m_boardThemeGroup, themes::boardThemes(), true);
    addThemes(m_pieceSetMenu, m_pieceSetGroup, themes::pieceSets(), false);
    m_board->setBoardTheme(s.value(QStringLiteral("board/theme"), themes::defaultBoard()).toString());
    m_board->setPieceSet(s.value(QStringLiteral("board/pieces"), themes::defaultPieces()).toString());
    for (QAction *a : m_boardThemeGroup->actions())
        a->setChecked(a->data().toString() == m_board->boardTheme());
    for (QAction *a : m_pieceSetGroup->actions())
        a->setChecked(a->data().toString() == m_board->pieceSet());

    // Sounds: both optional, saved; hidden when the program was built without Qt Multimedia
    m_effectsAction->setCheckable(true);
    m_speechAction->setCheckable(true);
    m_effectsAction->setChecked(s.value(QStringLiteral("sound/effects"), false).toBool());
    m_speechAction->setChecked(s.value(QStringLiteral("sound/speech"), false).toBool());
    m_sound->setEffectsEnabled(m_effectsAction->isChecked());
    m_sound->setSpeechEnabled(m_speechAction->isChecked());
    m_effectsAction->setVisible(SoundPlayer::isAvailable());
    m_speechAction->setVisible(SoundPlayer::isAvailable());
    m_voiceMenu->menuAction()->setVisible(SoundPlayer::isAvailable());

    // Voice: by the program language, or a fixed one
    m_voiceGroup = new QActionGroup(this);
    const QString voice = s.value(QStringLiteral("sound/voice"), QStringLiteral("auto")).toString();
    for (const char *id : { "auto", "thorsten-de", "libritts-en", "amichess" }) {
        QAction *a = m_voiceMenu->addAction(QString());
        a->setData(QLatin1String(id));
        a->setCheckable(true);
        a->setChecked(voice == QLatin1String(id));
        m_voiceGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, a] {
            QSettings().setValue(QStringLiteral("sound/voice"), a->data().toString());
            updateSpeechLanguage();
        });
        if (qstrcmp(id, "auto") == 0)
            m_voiceMenu->addSeparator();
    }
    // Own voices (tools/voicerecorder) in the folder themes/sounds next to the program
    bool separator = false;
    for (const QString &id : themes::soundSets()) {
        if (id == QLatin1String("effects") || id == QLatin1String("thorsten-de") || id == QLatin1String("libritts-en")
            || id == QLatin1String("amichess"))
            continue;
        const QString folder = themes::soundFolder(id);
        if (folder.startsWith(QLatin1Char(':')) || !QFileInfo::exists(folder + QStringLiteral("/a.wav")))
            continue;
        if (!separator) {
            m_voiceMenu->addSeparator();
            separator = true;
        }
        const QString name = QSettings(folder + QStringLiteral("/theme.ini"), QSettings::IniFormat)
                                 .value(QStringLiteral("theme/name"), id).toString();
        QAction *a = m_voiceMenu->addAction(name);
        a->setData(id);
        a->setCheckable(true);
        a->setChecked(voice == id);
        m_voiceGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, a] {
            QSettings().setValue(QStringLiteral("sound/voice"), a->data().toString());
            updateSpeechLanguage();
        });
    }
    if (!m_voiceGroup->checkedAction())
        m_voiceGroup->actions().constFirst()->setChecked(true);

    s.remove(QStringLiteral("sound/preloading")); // crash guard of older versions
    connect(m_effectsAction, &QAction::toggled, this, [this](bool on) {
        m_sound->setEffectsEnabled(on);
        if (on)
            m_sound->preload(m_sound->effectsFolder());
        QSettings().setValue(QStringLiteral("sound/effects"), on);
        updateSoundIcons();
    });
    connect(m_speechAction, &QAction::toggled, this, [this](bool on) {
        m_sound->setSpeechEnabled(on);
        if (on)
            m_sound->preload(m_sound->speechFolder());
        QSettings().setValue(QStringLiteral("sound/speech"), on);
        updateSoundIcons();
    });
    updateSoundIcons();
    updateSpeechLanguage();
    // Switched-on sounds load in the background, starting shortly after the window is up.
    if (m_sound->effectsEnabled())
        m_sound->preload(m_sound->effectsFolder());
    // Greeting ("welcome.wav" of the voice, e.g. recorded with tools/voicerecorder)
    if (m_sound->speechEnabled())
        QTimer::singleShot(700, this, [this] { m_sound->announce(QStringLiteral("welcome")); });
}

void MainWindow::updateSpeechLanguage()
{
    if (!m_sound)
        return;
    // Announcements in the chosen voice, or in the language of the program
    QString voice = m_voiceGroup && m_voiceGroup->checkedAction() ? m_voiceGroup->checkedAction()->data().toString()
                                                                  : QStringLiteral("auto");
    if (voice == QLatin1String("auto"))
        voice = m_languages->language() == QLatin1String("de") ? QStringLiteral("thorsten-de")
                                                               : QStringLiteral("libritts-en");
    m_sound->setSpeechSet(voice);
    if (m_sound->speechEnabled())
        m_sound->preload(m_sound->speechFolder());
}

QString MainWindow::themeDisplayName(const QString &id) const
{
    // Built-in names are translated; the others are shown as their theme.ini says.
    if (id == themes::defaultBoard())
        return tr("Grey (mbChess)");
    if (id == themes::classicBoard())
        return tr("mbChess (classic)");
    if (id == themes::defaultPieces())
        return tr("Wooden discs (mbChess)");
    if (id == themes::classicPieces())
        return tr("Classic (cburnett)");
    static const char *const names[][2] = {
        { "Crystal", QT_TR_NOOP("Crystal") },         { "Default", QT_TR_NOOP("AmiChess") },
        { "Fire", QT_TR_NOOP("Fire") },               { "Ice", QT_TR_NOOP("Ice") },
        { "MarbleBrown", QT_TR_NOOP("Marble, brown") }, { "MarbleGrey", QT_TR_NOOP("Marble, grey") },
        { "Metal", QT_TR_NOOP("Metal") },             { "Plasma", QT_TR_NOOP("Plasma") },
        { "Redwood", QT_TR_NOOP("Redwood") },         { "Water", QT_TR_NOOP("Water") },
        { "Wood1", QT_TR_NOOP("Wood, light") },       { "Wood2", QT_TR_NOOP("Wood, parquet") },
    };
    for (const auto &n : names)
        if (id == QLatin1String(n[0]))
            return tr(n[1]);
    return QString();
}

void MainWindow::retranslateThemeMenus()
{
    m_boardThemeMenu->setTitle(tr("Board &style"));
    m_pieceSetMenu->setTitle(tr("&Pieces"));
    m_effectsAction->setText(tr("Move &sounds"));
    m_speechAction->setText(tr("&Announce moves"));
    m_voiceMenu->setTitle(tr("&Voice"));
    m_dgtAction->setText(tr("Use the &DGT board"));
    m_dgtTakeAction->setText(tr("Take the position from the DG&T board..."));
    m_dgtSettingsAction->setText(tr("DGT boa&rd settings..."));
    updateDgtIcon();
    updateDgtStatus();
    const QStringList voiceNames = { tr("Automatic (program language)"), tr("German (Thorsten)"),
                                     tr("English (LibriTTS)"), tr("English (AmiChess original)") };
    const QList<QAction *> voices = m_voiceGroup->actions();
    for (int i = 0; i < voices.size() && i < voiceNames.size(); ++i)
        voices.at(i)->setText(voiceNames.at(i));
    for (QActionGroup *group : { m_boardThemeGroup, m_pieceSetGroup })
        for (QAction *a : group->actions()) {
            const QString translated = themeDisplayName(a->data().toString());
            a->setText(translated.isEmpty() ? a->property("themeName").toString() : translated);
        }
}

// ---------------------------------------------------------------------------
// Files, positions, input

void MainWindow::openPgn()
{
    if (!confirmEndGame(tr("Open game"), tr("Opening another game ends this game.")))
        return;
    QSettings s;
    const QString file = QFileDialog::getOpenFileName(this, tr("Open game"),
                                                      s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString(),
                                                      tr("PGN files (*.pgn);;All files (*)"));
    if (file.isEmpty())
        return;
    s.setValue(QStringLiteral("files/lastDir"), QFileInfo(file).absolutePath());
    openPgnFile(file);
}

bool MainWindow::openPgnFile(const QString &file)
{
    // The file is indexed (in the background if it is large); databaseLoaded() follows.
    if (!QFileInfo(file).isFile()) {
        QMessageBox::warning(this, tr("Open game"), tr("The file could not be opened:\n%1").arg(file));
        return false;
    }
    showStatus(tr("Reading \"%1\"...").arg(QFileInfo(file).fileName()));
    m_gameList->openFile(file);
    return true;
}

void MainWindow::databaseLoaded(int count)
{
    updateDatabaseActions();
    if (count == 0) {
        showStatus(QString());
        QMessageBox::warning(this, tr("Open game"), tr("The file contains no games."));
        return;
    }
    if (count > 1) {
        const bool wasHidden = m_gameListDock->isHidden();
        m_gameListDock->show();
        m_gameListDock->raise();
        // room for a few rows of the list
        if (wasHidden && !m_gameListDock->isFloating() && m_gameListDock->height() < 240)
            resizeDocks({ m_gameListDock }, { 240 }, Qt::Vertical);
    }
    loadDatabaseGame(0);
}

void MainWindow::loadDatabaseGame(int index)
{
    if (m_watching)
        m_benchmarkFollowAction->setChecked(false);
    Game game;
    QString error;
    if (!m_gameList->loadGame(index, &game, &error)) {
        QMessageBox::warning(this, tr("Open game"), tr("The game could not be read:\n%1").arg(error));
        return;
    }

    m_board->setFlipped(false);
    m_board->setAutoQueen(m_gm->settings().autoQueen);
    m_gm->loadGame(game);
    m_gameList->setCurrentGame(index);
    updateDatabaseActions();

    const QString file = QFileInfo(m_gameList->fileName()).fileName();
    const int count = m_gameList->gameCount();
    showStatus(count > 1 ? tr("Game %1 of %2 from \"%3\".").arg(index + 1).arg(count).arg(file)
                                       : tr("Loaded \"%1\".").arg(file),
                             8000);
    if (!error.isEmpty())
        QMessageBox::information(this, tr("Open game"), tr("Some moves could not be read:\n%1").arg(error));
}

void MainWindow::stepDatabaseGame(int step)
{
    const int game = m_gameList->neighbourGame(step);
    if (game >= 0 && confirmEndGame(tr("Open game"), tr("Opening another game ends this game.")))
        loadDatabaseGame(game);
}

void MainWindow::updateDatabaseActions()
{
    const bool many = m_gameList->gameCount() > 1 && !(m_benchmark && m_benchmark->isRunning());
    m_prevGameAction->setEnabled(many && m_gameList->neighbourGame(-1) >= 0);
    m_nextGameAction->setEnabled(many && m_gameList->neighbourGame(1) >= 0);
}

void MainWindow::savePgn()
{
    QSettings s;
    QString name = QStringLiteral("%1 - %2 %3.pgn")
                       .arg(m_game.tag(QStringLiteral("White")), m_game.tag(QStringLiteral("Black")),
                            QDate::currentDate().toString(Qt::ISODate));
    name.replace(QRegularExpression(QStringLiteral("[\\\\/:*?\"<>|]")), QStringLiteral("_"));
    const QString dir = s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString();
    QString file = QFileDialog::getSaveFileName(this, tr("Save game"), QDir(dir).filePath(name),
                                                tr("PGN files (*.pgn)"));
    if (file.isEmpty())
        return;
    if (!file.endsWith(QLatin1String(".pgn"), Qt::CaseInsensitive))
        file += QStringLiteral(".pgn");
    s.setValue(QStringLiteral("files/lastDir"), QFileInfo(file).absolutePath());

    QString error;
    if (!pgn::writeFile(file, m_game, &error)) {
        QMessageBox::warning(this, tr("Save game"), tr("The game could not be saved:\n%1").arg(error));
        return;
    }
    m_gm->markSaved();
    showStatus(tr("Game saved to \"%1\".").arg(QDir::toNativeSeparators(file)), 6000);
}



bool MainWindow::setPositionFromFen(const QString &fen, QString *error)
{
    Position p;
    if (!p.setFromFen(fen, error))
        return false;
    m_gm->usePosition(p);
    return true;
}



bool MainWindow::confirmEndGame(const QString &title, const QString &what, bool discards)
{
    // a running game ends; a game with unsaved moves or comments is lost (if "discards")
    const bool running = m_gm->gameInProgress();
    const bool unsaved = discards && m_gm->hasUnsavedChanges();
    if (!running && !unsaved)
        return true;
    QString text = running ? tr("<p>A game is running.</p><p>%1</p>").arg(what)
                           : tr("<p>Your game may contain unsaved data: moves, variations or comments that "
                                "are not saved in a PGN file yet. If you go on, they are lost.</p>");
    if (!unsaved)
        text += tr("<p>Save it first with File > Save game as PGN if you want to keep it.</p>");
    QMessageBox box(QMessageBox::Warning, title, text, QMessageBox::NoButton, this);
    QPushButton *save = unsaved ? box.addButton(tr("&Save game..."), QMessageBox::ActionRole) : nullptr;
    QPushButton *go = box.addButton(unsaved ? tr("&Discard") : tr("&Continue"), QMessageBox::AcceptRole);
    QPushButton *back = box.addButton(running ? tr("&Back to the game") : tr("&Cancel"), QMessageBox::RejectRole);
    box.setDefaultButton(save ? save : back);
    box.setEscapeButton(back);
    box.exec();
    if (save && box.clickedButton() == save) {
        savePgn();
        return !m_gm->hasUnsavedChanges(); // saved: go on; dialog cancelled: stay
    }
    return box.clickedButton() == go;
}

void MainWindow::toAnalysisBoard()
{
    m_gm->stopPlaying();
    m_gm->clock()->setTimeControl(TimeControl());
    m_gm->updatePlayerNames();
    updateView();
}

void MainWindow::setupPosition()
{
    if (!confirmEndGame(tr("Set up position"),
                        tr("If you apply a new position in the board editor, the game is ended. "
                           "With \"Discard\" it goes on.")))
        return;
    // An empty board to set up on; "Current position" in the dialog brings the game's back.
    BoardEditorDialog dlg(Position(), m_game.position(), m_board->boardTheme(), m_board->pieceSet(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_gm->usePosition(dlg.position());
    showStatus(tr("Position set up. Play it with File > New game, or look for a mate with "
                                "Engines > Mate search."), 10000);
}

void MainWindow::mateSearch()
{
    if (!confirmEndGame(tr("Mate search"),
                        tr("Help from an engine ends the game: the board becomes an analysis board."), false))
        return;
    if (m_gm->gameInProgress())
        toAnalysisBoard();
    const bool wasAnalysing = m_analysisAction->isChecked();
    if (wasAnalysing)
        m_analysisAction->setChecked(false); // the engines would share the processor
    MateSearchDialog dlg(m_game.position(), m_languages->pieceLetters(), m_gm->settings().threads, m_gm->settings().hashMb,
                         !m_watching && !m_gm->isPlayingMode(), this);
    const bool insert = dlg.exec() == QDialog::Accepted && !dlg.solution().isEmpty();
    if (insert && !m_watching && !m_gm->isPlayingMode()) {
        const Game::NodeId oldEnd = m_gm->mainLineEnd();
        const Game::NodeId from = m_game.currentNode();
        Game::NodeId first = -1;
        for (const QString &u : dlg.solution()) {
            const Move m = parseUci(m_game.position(), u);
            if (m.isNull() || !m_game.addMove(m))
                break;
            if (first < 0)
                first = m_game.currentNode();
        }
        if (first >= 0 && m_game.comment(first).isEmpty())
            m_game.setComment(first, dlg.solutionComment());
        m_game.goToNode(from);
        m_gm->afterTreeChange(oldEnd);
        showStatus(tr("The solution was added to the game (step through it with the right arrow key)."), 8000);
    }
    if (wasAnalysing && analysisAllowed())
        m_analysisAction->setChecked(true);
}

// ---------------------------------------------------------------------------
// DGT e-board

#ifdef MBCHESS_HAVE_DGT
namespace {
QVector<Piece> placementOf(const Position &pos)
{
    QVector<Piece> v(64, NoPiece);
    for (int sq = 0; sq < 64; ++sq)
        v[sq] = pos.pieceAt(sq);
    return v;
}
} // namespace
#endif

void MainWindow::updateDgtIcon()
{
    if (!m_dgtAction)
        return;
    // The check mark and the icon show a connected board; "wanted" alone (switched on, but no
    // board found yet) looks like off, with a tooltip that mbChess waits for the board.
#ifdef MBCHESS_HAVE_DGT
    const bool connected = m_dgt && m_dgt->isConnected();
#else
    const bool connected = false;
#endif
    m_dgtAction->setChecked(connected); // no signal: setDgt() follows only the user's clicks
    m_dgtAction->setIcon(guitheme::toolbarIcon(connected ? QStringLiteral("dgt") : QStringLiteral("dgt-off")));
    m_dgtAction->setToolTip(connected ? tr("DGT board on (Ctrl+D): moves on the board are played here")
                            : dgtWanted() ? tr("No DGT board connected: mbChess uses it as soon as it is "
                                               "plugged in or switched on (Ctrl+D: search now)")
                                          : tr("DGT board off (Ctrl+D)"));
}

bool MainWindow::dgtWanted() const
{
#ifdef MBCHESS_HAVE_DGT
    return dgtsettings::enabled();
#else
    return false;
#endif
}

void MainWindow::setDgt(bool on)
{
#ifdef MBCHESS_HAVE_DGT
    // A click on the unchecked entry while the board is wanted but not there: search again.
    dgtsettings::setEnabled(on);
    m_dgtUserOff = !on;
    if (on) {
        dgtStartupCheck(false);
    } else {
        m_dgtScanner->cancel();
        m_dgt->close();
    }
#else
    if (on) {
        const QSignalBlocker block(m_dgtAction);
        m_dgtAction->setChecked(false);
        QMessageBox::information(this, tr("DGT board"),
                                 tr("This mbChess was built without Qt Serial Port, so it cannot talk to a DGT "
                                    "board. Add \"Qt Serial Port\" to the Qt kit with the Qt Maintenance Tool "
                                    "and build mbChess again (qmake, then build)."));
    }
#endif
    updateDgtIcon();
    updateDgtStatus();
}

bool MainWindow::dgtSearching() const
{
#ifdef MBCHESS_HAVE_DGT
    return m_dgtScanner && m_dgtScanner->isRunning();
#else
    return false;
#endif
}

QStringList MainWindow::dgtPortNames() const
{
    QStringList names;
#ifdef MBCHESS_HAVE_DGT
    for (const dgt::PortInfo &p : dgt::availablePorts())
        names << p.name;
#endif
    return names;
}

bool MainWindow::dgtStartupCheck(bool quiet)
{
#ifdef MBCHESS_HAVE_DGT
    // first the stored port, then "blind" all others (new port, other board)
    if (m_dgt->isOpen() || m_dgtDialogOpen)
        return false;
    const bool want = dgtWanted() || (dgtsettings::autoDetect() && !m_dgtUserOff);
    if (!want)
        return false;
    QVector<dgt::PortInfo> ports;
    const QString stored = dgtsettings::port();
    const QVector<dgt::PortInfo> all = dgt::availablePorts();
    if (!stored.isEmpty()) {
        dgt::PortInfo p;
        p.name = stored;
        for (const dgt::PortInfo &i : all)
            if (i.name == stored)
                p = i;
        ports << p;
    }
    if (stored.isEmpty() || dgtsettings::autoScan())
        for (const dgt::PortInfo &i : all)
            if (i.name != stored)
                ports << i;
    if (ports.isEmpty()) {
        if (dgtWanted()) {
            m_dgtStatus->setText(tr("no board found"));
            m_dgtStatus->setToolTip(tr("No serial port found."));
        }
        return false;
    }
    dgtSearch(ports, quiet);
    return true;
#else
    Q_UNUSED(quiet)
    return false;
#endif
}

#ifdef MBCHESS_HAVE_DGT
void MainWindow::dgtSearch(const QVector<dgt::PortInfo> &ports, bool quiet)
{
    if (m_dgtScanner->isRunning() || ports.isEmpty())
        return;
    m_dgtQuiet = quiet;
    m_dgtFoundPort.clear();
    if (dgtWanted() && !quiet) {
        m_dgtStatus->setText(tr("looking for the board ..."));
        m_dgtStatus->setToolTip(QString());
    }
    updateDgtStatus();
    m_dgtScanner->scan(ports);
}

void MainWindow::dgtSearchFinished()
{
    if (m_dgtDialogOpen || m_dgt->isOpen())
        return;
    const bool want = dgtWanted() || (dgtsettings::autoDetect() && !m_dgtUserOff);
    if (!m_dgtFoundPort.isEmpty() && want) {
        const int orientation = dgtsettings::orientation();
        m_dgt->setAutoFlip(orientation == 0);
        if (orientation != 0)
            m_dgt->setFlipped(orientation == 2);
        QString error;
        if (m_dgt->open(m_dgtFoundPort, &error)) {
            dgtsettings::setPort(m_dgtFoundPort);
            dgtsettings::setEnabled(true);
            updateDgtIcon();
            m_dgtStatus->setText(tr("connecting to %1 ...").arg(m_dgtFoundPort));
            m_dgtStatus->setToolTip(QString());
            updateDgtStatus();
            return;
        }
        m_dgtStatus->setText(tr("not connected"));
        m_dgtStatus->setToolTip(error);
        return;
    }
    if (!dgtWanted())
        return;
    m_dgtStatus->setText(tr("no board found"));
    m_dgtStatus->setToolTip(tr("mbChess connects as soon as the board is plugged in or switched on."));
    if (!m_dgtQuiet) {
        const auto answer = QMessageBox::question(
            this, tr("DGT board"),
            tr("No DGT board was found. Is it connected and switched on?\n\nOpen the DGT board settings?"));
        if (answer == QMessageBox::Yes)
            dgtDialog();
    }
}
#endif

void MainWindow::dgtWatch()
{
#ifdef MBCHESS_HAVE_DGT
    const QStringList current = dgtPortNames();
    QStringList added;
    for (const QString &name : current)
        if (!m_dgtKnownPorts.contains(name))
            added << name;
    m_dgtKnownPorts = current;
    if (m_dgt->isOpen() || m_dgtScanner->isRunning() || m_dgtDialogOpen)
        return;
    const bool want = dgtWanted() || (dgtsettings::autoDetect() && !m_dgtUserOff);
    if (!want)
        return;
    QVector<dgt::PortInfo> ports;
    for (const dgt::PortInfo &p : dgt::availablePorts())
        if (added.contains(p.name))
            ports << p;
    // a board that was switched off (Bluetooth: the port stays) is asked again every 6 s
    if (ports.isEmpty() && dgtWanted() && ++m_dgtWatchTicks % 3 == 0 && !dgtsettings::port().isEmpty()) {
        dgt::PortInfo p;
        p.name = dgtsettings::port();
        ports << p;
    }
    dgtSearch(ports, true);
#endif
}

void MainWindow::dgtDialog()
{
#ifdef MBCHESS_HAVE_DGT
    // the dialog probes the ports: free ours
    m_dgtScanner->cancel();
    const bool wasOn = dgtWanted();
    m_dgt->close();
    m_dgtDialogOpen = true;
    DgtDialog dlg(this);
    const bool accepted = dlg.exec() == QDialog::Accepted;
    m_dgtDialogOpen = false;
    const bool on = accepted ? dlg.useBoard() : wasOn;
    m_dgtUserOff = !on;
    dgtsettings::setEnabled(on);
    if (on)
        dgtStartupCheck(false);
    updateDgtIcon();
    updateDgtStatus();
#else
    setDgt(true);
#endif
}

void MainWindow::onDgtBoardChanged()
{
#ifdef MBCHESS_HAVE_DGT
    if (!m_dgt->isConnected())
        return;
    if (!dgtPauseReason().isEmpty()) { // benchmark or engine against engine: the board keeps out
        updateDgtStatus();
        return;
    }
    const QVector<Piece> board = m_dgt->pieces();
    const Position &pos = m_game.position();
    // a move on the board: the position after exactly one legal move
    if (board != placementOf(pos) && m_gm->humanMayMove() && !QApplication::activeModalWidget()) {
        for (const Move &m : pos.legalMoves()) {
            if (placementOf(pos.after(m)) == board) {
                onBoardMove(m);
                return; // updateView() shows the state
            }
        }
    }
#endif
    updateDgtStatus();
}

QString MainWindow::dgtPauseReason() const
{
    if (m_watching || (m_benchmark && m_benchmark->isRunning()))
        return tr("paused while the benchmark runs");
    if (m_gm->settings().engineVsEngine() && !m_gm->isGameOver() && !m_gm->isWaitingForGame())
        return tr("paused: engine against engine");
    return QString();
}

void MainWindow::showStatus(const QString &text, int timeout)
{
    m_statusMessage = text;
    m_statusField->setText(text.isEmpty() ? tr("Ready") : text);
    if (timeout > 0 && !text.isEmpty())
        m_statusTimer->start(timeout);
    else
        m_statusTimer->stop();
}

void MainWindow::updateDgtStatus()
{
    if (!m_dgtStatus)
        return;
    updateDgtIcon(); // connected or not
    // Taking the position needs a connected board, and no benchmark that owns the board.
#ifdef MBCHESS_HAVE_DGT
    const bool boardThere = m_dgt && m_dgt->isConnected() && dgtWanted();
#else
    const bool boardThere = false;
#endif
    m_dgtTakeAction->setEnabled(boardThere && !(m_benchmark && m_benchmark->isRunning()));
    if (!dgtWanted()) {
        m_dgtStatus->hide();
        return;
    }
    m_dgtStatus->show();
#ifdef MBCHESS_HAVE_DGT
    if (!m_dgt || !m_dgt->isConnected())
        return; // "connecting", "looking", "not connected" stay
    QString tip = tr("DGT board on %1").arg(m_dgt->portName());
    if (!m_dgt->version().isEmpty())
        tip += QLatin1Char('\n') + tr("Version %1").arg(m_dgt->version());
    if (m_dgt->isFlipped())
        tip += QLatin1Char('\n') + tr("Black at the connector side");
    const QString paused = dgtPauseReason();
    if (!paused.isEmpty()) {
        m_dgtStatus->setText(paused);
        m_dgtStatus->setToolTip(tip + QLatin1Char('\n')
                                + tr("Moves on the board are ignored until the run is over."));
        return;
    }
    const QVector<Piece> board = m_dgt->pieces();
    const Position &pos = m_game.position();
    const QVector<Piece> game = placementOf(pos);
    if (board == game) {
        m_dgtStatus->setText(tr("\u2713 matches the game"));
        tip += QLatin1Char('\n') + tr("The board matches the game.");
    } else {
        const Game::NodeId node = m_game.currentNode();
        if (node != Game::RootNode && board == placementOf(m_game.nodePosition(m_game.parent(node)))) {
            const QString san = localizeSan(m_game.nodeSan(node), m_languages->pieceLetters());
            m_dgtStatus->setText(tr("please play %1 on the board").arg(san));
        } else {
            int diff = 0;
            for (int sq = 0; sq < 64; ++sq)
                diff += board.at(sq) != game.at(sq);
            m_dgtStatus->setText(tr("the board differs on %n square(s)", nullptr, diff));
            tip += QLatin1Char('\n') + tr("Board: %1").arg(dgt::placementFen(board));
        }
    }
    m_dgtStatus->setToolTip(tip);
#else
    m_dgtStatus->setText(tr("not available"));
#endif
}

void MainWindow::dgtTakePosition()
{
#ifdef MBCHESS_HAVE_DGT
    if (!m_dgt->isConnected()) {
        QMessageBox::information(this, tr("DGT board"), tr("No DGT board is connected (Board > Use the DGT board)."));
        return;
    }
    if (!dgtPauseReason().isEmpty()) {
        QMessageBox::information(this, tr("DGT board"),
                                 tr("The DGT board is paused while a benchmark or an engine-against-engine game "
                                    "runs."));
        return;
    }
    if (!confirmEndGame(tr("Set up position"),
                        tr("If you apply the position of the DGT board, the game is ended. "
                           "With \"Discard\" it goes on.")))
        return;
    Position p;
    const QVector<Piece> board = m_dgt->pieces();
    for (int sq = 0; sq < 64; ++sq)
        p.setPiece(sq, board.at(sq));
    p.setSideToMove(BoardEditorDialog::automaticSideToMove(p));
    p.setCastlingRights(WhiteKingSide | WhiteQueenSide | BlackKingSide | BlackQueenSide);
    BoardEditorDialog dlg(p, m_board->boardTheme(), m_board->pieceSet(), this);
    if (dlg.exec() != QDialog::Accepted)
        return;
    m_gm->usePosition(dlg.position());
#else
    setDgt(true);
#endif
}

void MainWindow::setFenDialog()
{
    if (!confirmEndGame(tr("Set position"), tr("A new position from a FEN ends the game.")))
        return;
    bool ok = false;
    const QString fen = QInputDialog::getText(this, tr("Set position"), tr("FEN:"), QLineEdit::Normal,
                                              m_game.position().toFen(), &ok);
    if (!ok)
        return;
    QString error;
    if (!setPositionFromFen(fen, &error))
        QMessageBox::warning(this, tr("Invalid FEN"), tr("The FEN could not be read:\n%1").arg(error));
}

void MainWindow::copyFen()
{
    QApplication::clipboard()->setText(m_game.position().toFen());
    showStatus(tr("FEN copied to the clipboard."), 3000);
}

void MainWindow::enterMove()
{
    QString text = m_input->text().trimmed();
    if (text.isEmpty())
        return;
    if (!m_gm->humanMayMove()) {
        if (m_gm->isGameOver() && m_game.currentNode() == m_gm->mainLineEnd())
            showStatus(tr("The game is over."), 3000);
        else if (m_gm->isPlayingMode() && !m_game.isAtLineEnd())
            showStatus(tr("Go to the end of the game to make a move."), 3000);
        else
            showStatus(tr("Please wait, the engine is to move."), 3000);
        return;
    }
    // Accept local piece letters (e.g. German "Sf3") by mapping them to English first.
    const QString local = m_languages->pieceLetters();
    const QString english = QStringLiteral("KQRBN");
    if (local.size() == 5 && text.at(0).isUpper() && local.contains(text.at(0)))
        text[0] = english.at(local.indexOf(text.at(0)));

    Move m = parseSan(m_game.position(), text);
    if (m.isNull())
        m = parseUci(m_game.position(), text);
    if (m.isNull()) {
        showStatus(tr("\"%1\" is not a legal move here.").arg(m_input->text()), 4000);
        return;
    }
    m_input->clear();
    m_gm->applyMove(m);
}

void MainWindow::flipBoard()
{
    m_board->setFlipped(!m_board->isFlipped());
}

QString MainWindow::manualFile() const
{
    // Installed by the Windows installer as <program folder>/Handbuch/mbChess-Handbuch.pdf
    // or Manual/mbChess-Manual.pdf; the build copies both next to the program.
    const bool german = m_languages->language() == QLatin1String("de");
    const QStringList bases = german
        ? QStringList{ QStringLiteral("Handbuch/mbChess-Handbuch"), QStringLiteral("Manual/mbChess-Manual") }
        : QStringList{ QStringLiteral("Manual/mbChess-Manual"), QStringLiteral("Handbuch/mbChess-Handbuch") };
    QStringList folders{ QCoreApplication::applicationDirPath() };
#ifdef Q_OS_MACOS
    folders << QCoreApplication::applicationDirPath() + QStringLiteral("/../Resources")
            << QCoreApplication::applicationDirPath() + QStringLiteral("/../../.."); // next to the bundle
#endif
#ifdef Q_OS_LINUX
    folders << QStringLiteral("/usr/share/doc/mbchess");
#endif
    QStringList suffixes;
#ifdef Q_OS_WIN
    suffixes << QStringLiteral(".chm"); // the compiled help comes first on Windows
#endif
    suffixes << QStringLiteral(".pdf");
    for (const QString &suffix : suffixes)
        for (const QString &base : bases)
            for (const QString &folder : folders) {
                const QString path = QDir(folder).filePath(base + suffix);
                if (QFileInfo::exists(path))
                    return QDir::cleanPath(path);
                // also directly in the folder (e.g. /usr/share/doc/mbchess/mbChess-Manual.pdf)
                const QString flat = QDir(folder).filePath(QFileInfo(base).fileName() + suffix);
                if (QFileInfo::exists(flat))
                    return QDir::cleanPath(flat);
            }
    return QString();
}

void MainWindow::showManual()
{
    const QString file = manualFile();
    if (file.isEmpty()) {
        QMessageBox::information(this, tr("Manual"),
                                 tr("The manual was not found. It belongs in the folder \"%1\" next to the program.")
                                     .arg(m_languages->language() == QLatin1String("de") ? QStringLiteral("Handbuch")
                                                                                          : QStringLiteral("Manual")));
        return;
    }
#ifdef Q_OS_WIN
    // The .chm goes through the HTML Help API: it keeps a single help window per program and
    // only brings it to the front again, where opening the file would start one hh.exe each time.
    if (file.endsWith(QLatin1String(".chm"), Qt::CaseInsensitive) && htmlhelp::show(file))
        return;
#endif
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(file)))
        QMessageBox::warning(this, tr("Manual"),
                             tr("No program to show \"%1\" was found. Please install a PDF viewer.")
                                 .arg(QDir::toNativeSeparators(file)));
}

void MainWindow::showAbout()
{
    QDialog dialog(this);
    dialog.setWindowTitle(tr("About mbChess"));

    // Picture on the left, scaled to the height of the text (sharp on HiDPI screens)
    auto *picture = new QLabel(&dialog);
    QPixmap pixmap(QStringLiteral(":/images/about.jpg"));
    const qreal dpr = devicePixelRatioF();
    const int height = 315; // same size as in the installer
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaledToHeight(qRound(height * dpr), Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);
        picture->setPixmap(pixmap);
    }
    picture->setFrameShape(QFrame::StyledPanel);

    auto *text = new QLabel(&dialog);
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    text->setOpenExternalLinks(true);
    text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    text->setMinimumWidth(300);
    text->setText(tr("<h3>mbChess %1</h3>"
                     "<p>A chess program for Windows, Linux, macOS and Android.</p>"
                     "<p>&copy; Michael Bergmann 2026</p>"
                     "<p>Licensed under the GNU General Public License, version 3 or later.</p>"
                     "<p>Chess pieces: \"cburnett\" by Colin M.L. Burnett (GPL-2.0-or-later).</p>"
                     "<p>Built with Qt %2.</p>")
                      .arg(tr("%1 \"%2\"", "version number and version name")
                               .arg(QApplication::applicationVersion(), QStringLiteral(MBCHESS_CODENAME)),
                           QString::fromLatin1(qVersion())));

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok, &dialog);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);

    auto *right = new QVBoxLayout;
    right->addWidget(text, 1);
    right->addWidget(buttons);
    auto *layout = new QHBoxLayout(&dialog);
    layout->setSpacing(16);
    layout->addWidget(picture, 0, Qt::AlignTop);
    layout->addLayout(right, 1);
    dialog.exec();
}

// Opening book and opening names






void MainWindow::updateBenchmarkActions()
{
    const bool running = m_benchmark->isRunning();
    m_benchmarkStartAction->setEnabled(!running);
    m_benchmarkCancelAction->setEnabled(running);
    m_benchmarkResultsAction->setEnabled(running || !m_benchmark->games().isEmpty());
    // While the benchmark runs the board and the engines belong to it: everything that would
    // start or load another game, or change the engines, is greyed out until it is over.
    for (QAction *action : { m_newGameAction, m_engineVsEngineAction, m_openAction, m_setFenAction,
                             m_setupPositionAction, m_mateSearchAction, m_manageEnginesAction }) {
        if (action)
            action->setEnabled(!running);
    }
    updateDatabaseActions();
    updateDgtStatus(); // the DGT board pauses during a benchmark
}


void MainWindow::updateBenchmarkStatus()
{
    if (!m_benchmark->isRunning())
        return;
    const int index = m_benchmark->currentIndex();
    QString text = tr("%1 vs. %2  (%3/%4)")
                       .arg(m_benchmark->currentWhite(), m_benchmark->currentBlack())
                       .arg(index + 1)
                       .arg(m_benchmark->totalGames());
    QString tip = tr("Game %1 of %2: %3 against %4")
                      .arg(index + 1)
                      .arg(m_benchmark->totalGames())
                      .arg(m_benchmark->currentWhite(), m_benchmark->currentBlack());
    const qint64 remaining = m_benchmark->remainingMs();
    if (remaining >= 0) {
        text += QStringLiteral(" \u2013 ") + tr("about %1 left").arg(BenchmarkDialog::durationText(remaining));
        tip += QLatin1Char('\n') + tr("Expected end: about %1").arg(BenchmarkDialog::endTimeText(remaining));
    }
    m_benchmarkProgress->setFormat(text);
    m_benchmarkProgress->setToolTip(tip);
}

void MainWindow::startBenchmark()
{
    if (m_benchmark->isRunning())
        return;
    if (enginemanager::engines().size() < 2) {
        QMessageBox::information(this, tr("Engine benchmark"),
                                 tr("The benchmark needs at least two engines (menu Engines > Manage engines)."));
        return;
    }
    BenchmarkDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    const BenchmarkSettings settings = dialog.settings();
    // following the benchmark takes the board: a running game ends (warn first)
    if (settings.followOnBoard
        && !confirmEndGame(tr("Engine benchmark"),
                           tr("If you follow the benchmark on the board, this game is <b>ended</b>: the board "
                              "shows the benchmark games from the initial position.")))
        return;
    const QString duration = BenchmarkDialog::durationText(BenchmarkRunner::estimatedMs(settings));
    const auto answer = QMessageBox::question(
        this, tr("Engine benchmark"),
        tr("<p>The benchmark plays %1 games and can take a long time: up to about <b>%2</b> "
           "(finished about %3).</p>"
           "<p>The engines use the computer heavily meanwhile. You follow the progress in the status "
           "bar and can cancel the benchmark at any time (menu File > Benchmarks or the button next "
           "to the progress bar).</p>"
           "<p><b>While the benchmark runs</b> you cannot start a new game, open games, set up a "
           "position, paste a FEN, search for mates, take the position from the DGT board or manage "
           "the engines. These menu items are greyed out until the benchmark is finished or "
           "cancelled. The DGT board pauses meanwhile.</p><p>Start the benchmark now?</p>")
            .arg(BenchmarkRunner::gameCount(settings))
            .arg(duration, BenchmarkDialog::endTimeText(BenchmarkRunner::estimatedMs(settings))),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);
    if (answer != QMessageBox::Yes)
        return;
    m_benchmark->start(settings);
    if (m_benchmarkFollowAction->isChecked() != settings.followOnBoard)
        m_benchmarkFollowAction->setChecked(settings.followOnBoard); // starts following via toggled()
    else if (settings.followOnBoard)
        startWatching();
    updateBenchmarkActions();
}

void MainWindow::setFollowBenchmark(bool on)
{
    if (on && m_benchmark->isRunning() && !m_watching
        && !confirmEndGame(tr("Engine benchmark"),
                           tr("If you follow the benchmark on the board, this game is <b>ended</b>: the board "
                              "shows the benchmark games from the initial position."))) {
        const QSignalBlocker block(m_benchmarkFollowAction);
        m_benchmarkFollowAction->setChecked(false);
        return;
    }
    QSettings().setValue(QStringLiteral("benchmark/follow"), on);
    if (on && m_benchmark->isRunning())
        startWatching();
    else if (!on)
        stopWatching();
}

void MainWindow::startWatching()
{
    if (m_watching)
        return;
    // The board belongs to the benchmark now: the own game and its engines stop.
    m_gm->stopPlaying();
    m_gm->setSuspended(true); // no moves, no engine, no time forfeits of the own game
    m_watching = true;
    m_watchIndex = -1;
    m_benchmark->setPauseBetweenGames(3000); // time to look at the end of a game
    // clear the board at once: the old game must not stay while the engines start
    m_game.reset();
    m_gm->setGameOver(false);
    m_gm->setEngineLine(QString());
    m_gm->clock()->setTimeControl(TimeControl());
    m_clockWidget->setNames(tr("White"), tr("Black"));
    m_watchTitle = tr("Engine benchmark");
    syncBenchmarkGame();
    updateView();
}

void MainWindow::stopWatching()
{
    if (!m_watching)
        return;
    m_watching = false;
    m_gm->setSuspended(false);
    m_benchmark->setPauseBetweenGames(300);
    m_gm->clock()->stop();
    m_gm->setEngineLine(QString());
    m_gm->clock()->setTimeControl(TimeControl());
    updateView();
}

void MainWindow::syncBenchmarkGame()
{
    if (!m_watching || !m_benchmark->isInGame())
        return;
    const bool newGame = m_benchmark->currentIndex() != m_watchIndex;
    const Game::NodeId oldEnd = m_gm->mainLineEnd();
    const Game::NodeId oldNode = m_game.currentNode();
    const bool following = newGame || oldNode == oldEnd; // the user may look at earlier moves
    const int oldPlies = newGame ? -1 : m_game.plyCount();
    m_game = m_benchmark->currentGame();
    m_game.goToNode(following || !m_game.isValidNode(oldNode) ? m_gm->mainLineEnd() : oldNode);
    if (newGame) {
        m_watchIndex = m_benchmark->currentIndex();
        m_gm->setGameOver(false);
        m_gm->setEngineLine(QString());
        const BenchmarkSettings &s = m_benchmark->settings();
        TimeControl tc;
        tc.baseMs = s.baseMs;
        tc.incrementMs = s.incrementMs;
        m_gm->clock()->setTimeControl(tc);
        m_clockWidget->setNames(m_benchmark->currentWhite(), m_benchmark->currentBlack());
    }
    m_watchTitle = tr("Benchmark game %1 of %2: %3 against %4")
                       .arg(m_benchmark->currentIndex() + 1)
                       .arg(m_benchmark->totalGames())
                       .arg(m_benchmark->currentWhite(), m_benchmark->currentBlack());
    // clocks: the benchmark's own times, the side that thinks counts down
    m_gm->clock()->stop();
    m_gm->clock()->setRemaining(White, m_benchmark->clockMs(0));
    m_gm->clock()->setRemaining(Black, m_benchmark->clockMs(1));
    const int side = m_benchmark->thinkingSide();
    if (side >= 0)
        m_gm->clock()->start(side == 0 ? White : Black);
    if (oldPlies >= 0 && m_game.plyCount() > oldPlies) {
        m_sound->playMove(m_game.nodeSan(m_gm->mainLineEnd()));
        m_gm->setEngineLine(QString());
    }
    updateView();
}

void MainWindow::benchmarkGameFinished(const BenchmarkGame &game)
{
    if (!m_watching)
        return;
    const Game::NodeId oldNode = m_game.currentNode();
    const bool following = oldNode == m_gm->mainLineEnd();
    const int oldPlies = m_game.plyCount();
    m_game = game.game;
    m_game.goToNode(following || !m_game.isValidNode(oldNode) ? m_gm->mainLineEnd() : oldNode);
    if (m_game.plyCount() > oldPlies)
        m_sound->playMove(m_game.nodeSan(m_gm->mainLineEnd()));
    m_gm->clock()->stop();
    m_gm->setGameOver(true, game.reason + QLatin1Char('.'));
    updateView();
    showStatus(tr("%1 against %2: %3 (%4)")
                                 .arg(m_benchmark->settings().engines.at(game.white).name,
                                      m_benchmark->settings().engines.at(game.black).name,
                                      resultToString(game.result), game.reason),
                             8000);
}

void MainWindow::cancelBenchmark()
{
    if (!m_benchmark->isRunning())
        return;
    const auto answer = QMessageBox::question(this, tr("Engine benchmark"),
                                              tr("Cancel the running benchmark? The games played so far are kept."),
                                              QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (answer == QMessageBox::Yes)
        m_benchmark->cancel();
}

void MainWindow::showBenchmarkResults()
{
    if (!m_benchmarkWindow)
        m_benchmarkWindow = new BenchmarkWindow(m_benchmark, this);
    m_benchmarkWindow->refresh();
    m_benchmarkWindow->show();
    m_benchmarkWindow->raise();
    m_benchmarkWindow->activateWindow();
}

QString MainWindow::analysisEnginePath() const
{
    const QVector<EngineEntry> engines = enginemanager::engines();
    const QString saved = QSettings().value(QStringLiteral("analysis/engine")).toString();
    for (const EngineEntry &e : engines)
        if (e.path == saved)
            return saved;
    return engines.isEmpty() ? QString() : engines.constFirst().path; // Stockfish first
}

void MainWindow::fillAnalysisEngineMenu()
{
    m_analysisEngineMenu->clear();
    for (QAction *a : m_analysisEngineGroup->actions())
        m_analysisEngineGroup->removeAction(a);
    const QString current = analysisEnginePath();
    for (const EngineEntry &e : enginemanager::engines()) {
        QAction *a = m_analysisEngineMenu->addAction(e.name);
        a->setCheckable(true);
        a->setChecked(e.path == current);
        m_analysisEngineGroup->addAction(a);
        const QString path = e.path;
        connect(a, &QAction::triggered, this, [this, path] {
            QSettings().setValue(QStringLiteral("analysis/engine"), path);
            if (m_analysis->isRunning()) {
                m_analysis->stop();
                updateAnalysis(); // starts the new engine
            }
        });
    }
}

bool MainWindow::analysisAllowed() const
{
    // No help from the analysis while playing a game against an engine or the clock.
    return !m_watching && (!m_gm->isPlayingMode() || m_gm->isGameOver() || m_gm->isWaitingForGame());
}

void MainWindow::setAnalysis(bool on)
{
    if (!on) {
        m_analysis->stop();
        m_analysisNode = -1;
        m_gm->setEngineLine(QString());
        updateView();
        return;
    }
    if (!analysisAllowed()) {
        m_analysisAction->setChecked(false);
        showStatus(m_watching ? tr("No engine analysis while you follow the benchmark.")
                                            : tr("No engine analysis during a game: resign or finish it first."),
                                 8000);
        return;
    }
    if (analysisEnginePath().isEmpty()) {
        m_analysisAction->setChecked(false);
        QMessageBox::information(this, tr("Engine analysis"), tr("There is no engine (menu Engines > Manage engines)."));
        return;
    }
    // A finished game (or none yet) becomes an analysis board: moves add variations.
    if (m_gm->isPlayingMode() || m_gm->isWaitingForGame())
        m_gm->stopPlaying();
    updateView(); // starts the analysis
}

void MainWindow::updateAnalysis()
{
    if (!m_analysisAction || !m_analysisAction->isChecked())
        return;
    if (!analysisAllowed()) {
        m_analysisAction->setChecked(false); // a game was started: setAnalysis(false)
        return;
    }
    const EngineEntry entry = enginemanager::entry(analysisEnginePath());
    m_analysis->start(entry, m_gm->settings().threads, m_gm->settings().hashMb);
    m_analysisNode = m_game.currentNode();
    m_analysis->analyse(m_game.startPosition(), m_game.uciMoves());
}

QString MainWindow::analysisText() const
{
    if (m_analysis->isStarting())
        return tr("Starting the engine...");
    const QVector<EngineAnalysis::Line> &lines = m_analysis->lines();
    const Position &root = m_analysis->position();
    if (lines.isEmpty())
        return root.legalMoves().empty() ? tr("No moves possible in this position.") : tr("Analysing...");
    const QString letters = m_languages->pieceLetters();
    QStringList rows;
    for (const EngineAnalysis::Line &l : lines) {
        if (l.pv.isEmpty())
            continue;
        const int s = l.eval.value;
        QString score = l.eval.mate ? (s > 0 ? QStringLiteral("#%1").arg(s) : QStringLiteral("-#%1").arg(-s))
                                    : QStringLiteral("%1%2").arg(s >= 0 ? QStringLiteral("+") : QString())
                                          .arg(s / 100.0, 0, 'f', 2);
        QStringList moves;
        Position pos = root;
        for (int i = 0; i < l.pv.size() && i < 8; ++i) { // short enough for one row
            const Move m = parseUci(pos, l.pv.at(i));
            if (m.isNull())
                break;
            QString san = localizeSan(toSan(pos, m), letters);
            if (pos.sideToMove() == White)
                san = QStringLiteral("%1.").arg(pos.fullmoveNumber()) + san;
            else if (i == 0)
                san = QStringLiteral("%1...").arg(pos.fullmoveNumber()) + san;
            moves << san;
            pos.makeMove(m);
        }
        rows << QStringLiteral("%1  (%2)  %3").arg(score, tr("depth %1").arg(l.depth), moves.join(QLatin1Char(' ')));
    }
    return rows.join(QLatin1Char('\n'));
}

void MainWindow::onAnalysisUpdated()
{
    if (!m_analysis->isRunning())
        return;
    QString title = tr("Analysis: %1 (%n line(s))", nullptr, m_analysis->effectiveLines())
                        .arg(m_analysis->engineName());
    if (m_analysis->effectiveLines() < m_analysis->multiPv())
        title += QLatin1Char(' ') + tr("- this engine shows only one line");
    if (!m_analysis->lines().isEmpty() && m_analysis->lines().constFirst().nps > 0)
        title += QStringLiteral("  \u00b7  ") + tr("%1 kN/s").arg(m_analysis->lines().constFirst().nps / 1000);
    m_engineTitle->setText(title);
    m_engineOutput->setText(analysisText());
    // The best line's score goes to the move (evaluation graph, PGN), if it is deeper
    // than what is there. Scores from a PGN file without depth are kept.
    const QVector<EngineAnalysis::Line> &lines = m_analysis->lines();
    if (lines.isEmpty() || !lines.constFirst().eval.valid || lines.constFirst().depth < 10)
        return;
    const Game::NodeId node = m_analysisNode;
    if (node < 0 || !m_game.isValidNode(node) || node == Game::RootNode
        || m_game.nodePosition(node).toFen() != m_analysis->position().toFen())
        return;
    const Evaluation old = m_game.evaluation(node);
    if (old.valid && (old.depth == 0 || old.depth >= lines.constFirst().depth))
        return;
    m_game.setEvaluation(node, lines.constFirst().eval);
    m_graph->refresh();
}

void MainWindow::tablebaseDialog()
{
    TablebaseDialog dialog(this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    showStatus(tr("Endgame tablebases take effect with the next game."), 6000);
}

void MainWindow::manageEngines()
{
    EngineDialog dialog(m_gm->settings().enginePath, this);
    if (dialog.exec() != QDialog::Accepted)
        return;
    m_gm->updatePlayerNames();
    showStatus(tr("Changed engine settings take effect with the next game."), 6000);
}



void MainWindow::updateOpeningName()
{
    if (!m_openingLabel)
        return;
    if (!m_openingsLoaded) {
        m_openingsLoaded = true;
        for (const char *f : { "a", "b", "c", "d", "e" }) {
            QFile file(QStringLiteral(":/openings/%1.tsv").arg(QLatin1String(f)));
            if (file.open(QIODevice::ReadOnly))
                m_openings.addTsv(QString::fromUtf8(file.readAll()));
        }
        // German names (tools/translate-openings.py)
        QFile names(QStringLiteral(":/openings/names_de.tsv"));
        if (names.open(QIODevice::ReadOnly)) {
            const QStringList lines = QString::fromUtf8(names.readAll()).split(QLatin1Char('\n'));
            for (const QString &line : lines) {
                const int tab = line.indexOf(QLatin1Char('\t'));
                if (tab > 0)
                    m_openingNamesDe.insert(line.left(tab), line.mid(tab + 1).trimmed());
            }
        }
    }
    // the last named position on the way to the current move
    book::Opening opening;
    for (Game::NodeId n = m_game.currentNode(); n != Game::RootNode && !opening.isValid(); n = m_game.parent(n))
        opening = m_openings.find(m_game.nodePosition(n));
    QString name = opening.name;
    if (m_languages->language() == QLatin1String("de"))
        name = m_openingNamesDe.value(name, name);
    m_openingLabel->setText(opening.isValid() ? QStringLiteral("%1  %2").arg(opening.eco, name) : QStringLiteral("\u2013"));
    m_openingLabel->setToolTip(opening.isValid() ? opening.name : QString()); // the international name
}

QString MainWindow::statusText() const
{
    if (m_gm->isWaitingForGame())
        return tr("No game yet: <a href=\"new\">start a new game</a> (%1) or open one.")
            .arg(m_newGameAction->shortcut().toString(QKeySequence::NativeText));
    if (m_gm->isGameOver() && m_game.currentNode() == m_gm->mainLineEnd())
        return QStringLiteral("%1 %2").arg(m_gm->endReason(), resultToString(m_game.result()));
    if (m_watching) {
        QString s = m_watchTitle + QLatin1Char('.');
        if (!m_benchmark->isInGame() || m_benchmark->thinkingSide() < 0)
            s += QLatin1Char(' ') + tr("Starting the engines...");
        if (m_benchmark->isInGame() && m_benchmark->currentOpeningPlies() > 0)
            //: %1 = opening name, %2 = number of moves; the benchmark sets these moves up at once
            s += QLatin1Char(' ') + tr("Set opening: %1 (%2 moves, set up at once).")
                     .arg(m_benchmark->currentOpening())
                     .arg((m_benchmark->currentOpeningPlies() + 1) / 2);
        s += QLatin1Char(' ') + tr("You are watching: moves are not possible (menu File > Benchmarks).");
        return s;
    }
    const GameStatus status = m_game.status();
    if (status != GameStatus::Ongoing)
        return m_gm->statusReason(status);
    const Position &pos = m_game.position();
    const QString side = pos.sideToMove() == White ? tr("White") : tr("Black");
    QString s = pos.inCheck() ? tr("%1 to move, in check.").arg(side) : tr("%1 to move.").arg(side);
    if (m_gm->anyEngineThinking() && m_gm->isSearching())
        s += QLatin1Char(' ') + tr("The engine is thinking...");
    else if (m_gm->anyEngineStarting())
        s += QLatin1Char(' ') + tr("Starting the engine...");
    else if (m_gm->isPondering())
        s += QLatin1Char(' ') + tr("The engine thinks on your time (ponder).");
    if (m_gm->isPlayingMode() && !m_game.isAtLineEnd())
        s += QLatin1Char(' ') + tr("You are looking at an earlier move (End key: back to the game).");
    // Gaviota tables, only for analysis (no help during a game)
    if (!m_gm->isPlayingMode()) {
        const QString tb = tablebases::gaviotaText(pos);
        if (!tb.isEmpty())
            //: %1 = "White mates in 5 moves", "Draw" ...
            s += QLatin1Char(' ') + tr("Tablebase: %1.").arg(tb);
    }
    return s;
}

void MainWindow::updateView()
{
    if (!m_board)
        return;

    m_board->setPosition(m_game.position());
    m_board->setLastMove(m_game.lastMove());
    const bool mayMove = m_gm->humanMayMove();
    bool white = mayMove, black = mayMove;
    if (mayMove && m_gm->settings().hasEngine()) {
        white = !m_gm->settings().isEngineSide(White);
        black = !m_gm->settings().isEngineSide(Black);
    }
    m_board->setMovableColors(white, black);

    // Notation and evaluation graph
    m_notation->setPieceLetters(m_languages->pieceLetters());
    m_notation->setEditable(!m_gm->isPlayingMode() && !m_watching);
    m_notation->refresh();
    m_graph->refresh();

    // Engine panel
    if (m_analysis->isRunning()) {
        onAnalysisUpdated();
    } else if (m_watching) {
        const int side = m_benchmark->thinkingSide();
        QString title = m_watchTitle;
        if (side >= 0)
            title += QLatin1Char('\n') + tr("Engine: %1")
                         .arg(side == 0 ? m_benchmark->currentWhite() : m_benchmark->currentBlack());
        m_engineTitle->setText(title);
        m_engineOutput->setText(m_gm->engineLine());
    } else if (m_gm->settings().hasEngine() && !m_gm->settings().opponentName.isEmpty()) {
        const GameSettings &gs = m_gm->settings();
        QString title = gs.opponentElo > 0
            ? tr("Opponent: %1 (about %2 Elo)").arg(gs.opponentName).arg(gs.opponentElo)
            : tr("Opponent: %1").arg(gs.opponentName);
        if (gs.builtinPlayer != GameSettings::NoBuiltin)
            title += QLatin1Char('\n') + tr("Built-in player, no engine");
        else
            title += QLatin1Char('\n') + tr("Engine: %1").arg(m_gm->engine()->name());
        m_engineTitle->setText(title);
        m_engineOutput->setText(gs.builtinPlayer != GameSettings::NoBuiltin ? QString() : m_gm->engineLine());
    } else if (m_gm->settings().hasEngine()) {
        QString title = m_gm->settings().engineVsEngine()
            ? tr("Engine: %1").arg(m_gm->engineDisplayName(m_gm->engineFor(m_game.nodePosition(m_gm->mainLineEnd()).sideToMove())))
            : tr("Engine: %1").arg(m_gm->engine()->name());
        title += QStringLiteral(" (%1, %2)")
                     .arg(NewGameDialog::levelName(m_gm->settings().strengthElo),
                          tr("%n core(s)", nullptr, m_gm->settings().threads));
        m_engineTitle->setText(title);
        m_engineOutput->setText(m_gm->engineLine());
    } else if (m_gm->isWaitingForGame()) {
        m_engineTitle->setText(tr("No game started yet"));
        m_engineOutput->clear();
    } else {
        m_engineTitle->setText(tr("No engine (human against human)"));
        m_engineOutput->clear();
    }

    m_status->setText(statusText());
    updateOpeningName();
    m_fen->setText(QStringLiteral("FEN: ") + m_game.position().toFen());
    m_input->setEnabled(mayMove);
    m_moveButton->setEnabled(mayMove);
    const bool atStart = m_game.currentNode() == Game::RootNode;
    const bool atEnd = m_game.isAtLineEnd();
    m_takeBackAction->setEnabled(!m_watching && !atStart && atEnd && !(m_gm->settings().engineVsEngine() && m_gm->isPlayingMode()));
    m_moveNowAction->setEnabled(m_gm->anyEngineThinking());
    m_resignAction->setEnabled(!m_watching && !m_gm->settings().engineVsEngine() && !m_gm->isGameOver() && m_game.plyCount() > 0 && m_game.currentNode() == m_gm->mainLineEnd());
    m_startAction->setEnabled(!atStart);
    m_backAction->setEnabled(!atStart);
    m_forwardAction->setEnabled(!atEnd);
    m_endAction->setEnabled(!atEnd);
    updateAnalysis();
    updateDgtStatus();
}
