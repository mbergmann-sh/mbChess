// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - entry point

#include "betaexpiry.h"
#include "datafolders.h"
#include "tablebases.h"
#include "guitheme.h"
#include "languagemanager.h"
#include "mainwindow.h"
#include "splashscreen.h"

#include <compat/compat.h>

#include <QApplication>
#include <QCommandLineParser>
#include <QElapsedTimer>
#include <QIcon>
#include <QSettings>

#include <memory>

int main(int argc, char *argv[])
{
    mbc::compat::enableHighDpi();

    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("mbChess"));
    QApplication::setApplicationName(QStringLiteral("mbChess"));
    QApplication::setApplicationVersion(QStringLiteral(MBCHESS_VERSION));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/icons/mbchess.svg")));

    guitheme::apply(QSettings().value(QStringLiteral("ui/guiTheme"), guitheme::defaultId()).toString());

    datafolders::create();
    tablebases::openGaviotaForGui(); // positions with up to 5 pieces: result below the board

    LanguageManager languages;
    languages.setLanguage(LanguageManager::savedLanguage());

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("mbChess chess GUI"));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption fenOption(QStringLiteral("fen"),
                                       QStringLiteral("Start from the position given as FEN."),
                                       QStringLiteral("fen"));
    parser.addOption(fenOption);
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("A PGN file to open."), QStringLiteral("[file.pgn]"));
    parser.process(app);

    // test builds for beta testers: notice at the first start, nothing more after six weeks
    if (!beta::checkAtStartup())
        return 0;

    // start screen (View > Start screen), it shows what is loaded
    std::unique_ptr<SplashScreen> splash;
    if (SplashScreen::enabled()) {
        splash.reset(new SplashScreen);
        splash->show();
        startup::setSplash(splash.get());
        splash->step(5, QApplication::translate("main", "Settings and language"));
    }

    MainWindow window(&languages);
    if (parser.isSet(fenOption) && !window.setPositionFromFen(parser.value(fenOption)))
        qWarning("Ignoring invalid FEN: %s", qPrintable(parser.value(fenOption)));

    // DGT board: the stored port first, then all ports; at most a few seconds here,
    // a Bluetooth board may answer later (the search goes on in the background)
    if (window.dgtStartupCheck(true)) {
        startup::progress(80, QApplication::translate("main", "Looking for a DGT board ..."));
        QElapsedTimer waited;
        waited.start();
        while (window.dgtSearching() && waited.elapsed() < 4000)
            app.processEvents(QEventLoop::AllEvents, 50);
    }
    if (!parser.positionalArguments().isEmpty()) {
        startup::progress(90, QApplication::translate("main", "Opening the game file"));
        window.openPgnFile(parser.positionalArguments().constFirst());
    }
    startup::progress(100, QApplication::translate("main", "Ready"));
    window.show();
    if (splash) {
        splash->finish(&window);
        startup::setSplash(nullptr);
    }
    return app.exec();
}
