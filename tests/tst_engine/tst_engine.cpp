// SPDX-License-Identifier: GPL-3.0-or-later
// Tests for the engine module (UCI and XBoard). The live tests need Stockfish and
// Crafty (environment variable MBCHESS_CRAFTY) and are skipped without them.

#include <chesscore/notation.h>
#include <engine/uciengine.h>

#include <QtTest>

using namespace mbc;
using namespace mbc::engine;

class TestEngine : public QObject
{
    Q_OBJECT

private slots:
    void findEnginesInFolder()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        auto makeFile = [&](const QString &rel, bool executable) {
            QDir(dir.path()).mkpath(QFileInfo(rel).path());
            QFile f(dir.filePath(rel));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("x");
            f.close();
            if (executable)
                f.setPermissions(f.permissions() | QFileDevice::ExeOwner | QFileDevice::ExeUser);
        };
#ifdef Q_OS_WIN
        const QString exe = QStringLiteral(".exe");
#else
        const QString exe;
#endif
        makeFile(QStringLiteral("stockfish/stockfish") + exe, true);
        makeFile(QStringLiteral("stockfish/nn-1234.nnue"), false);
        makeFile(QStringLiteral("gnuchess/gnuchess") + exe, true);
        makeFile(QStringLiteral("crafty/crafty") + exe, true);   // XBoard engine
        makeFile(QStringLiteral("myengine") + exe, true);        // directly in the folder

        const QVector<EngineInfo> engines = findEngines(dir.path());
        QStringList names;
        for (const EngineInfo &e : engines)
            names << e.name;
        QCOMPARE(names, (QStringList{ QStringLiteral("Crafty"), QStringLiteral("GNU Chess"), QStringLiteral("myengine"),
                                      QStringLiteral("Stockfish") }));
        QVERIFY(engines.at(0).protocol == Protocol::XBoard);
        QCOMPARE(engines.at(1).arguments, QStringList{ QStringLiteral("--uci") });
        QVERIFY(engines.at(1).protocol == Protocol::Uci);
        QVERIFY(engines.at(3).arguments.isEmpty());
        QVERIFY(QFileInfo(engines.at(3).path).isAbsolute());
    }

    void infoLine()
    {
        SearchInfo i;
        QVERIFY(parseInfoLine(QStringLiteral("info depth 18 seldepth 25 multipv 1 score cp -34 upperbound nodes 123456 "
                                             "nps 987654 hashfull 12 time 125 pv e7e5 g1f3 b8c6"), &i));
        QCOMPARE(i.depth, 18);
        QCOMPARE(i.selDepth, 25);
        QVERIFY(i.hasScore);
        QVERIFY(!i.isMate);
        QCOMPARE(i.score, -34);
        QVERIFY(i.upperBound);
        QCOMPARE(i.nodes, qint64(123456));
        QCOMPARE(i.nps, qint64(987654));
        QCOMPARE(i.timeMs, qint64(125));
        QCOMPARE(i.hashFull, 12);
        QCOMPARE(i.pv, QStringList({ "e7e5", "g1f3", "b8c6" }));

        QVERIFY(parseInfoLine(QStringLiteral("info depth 30 score mate -3 pv a1a2"), &i));
        QVERIFY(i.isMate);
        QCOMPARE(i.score, -3);

        QVERIFY(!parseInfoLine(QStringLiteral("info string NNUE evaluation using nn.nnue"), &i));
        QVERIFY(!parseInfoLine(QStringLiteral("bestmove e2e4"), &i));
    }

    void optionLine()
    {
        EngineOption o;
        QVERIFY(parseOptionLine(QStringLiteral("option name Skill Level type spin default 20 min 0 max 20"), &o));
        QCOMPARE(o.name, QStringLiteral("Skill Level"));
        QCOMPARE(o.type, QStringLiteral("spin"));
        QCOMPARE(o.defaultValue, QStringLiteral("20"));
        QCOMPARE(o.min, QStringLiteral("0"));
        QCOMPARE(o.max, QStringLiteral("20"));

        QVERIFY(parseOptionLine(QStringLiteral("option name Style type combo default Normal var Solid var Normal var Risky"), &o));
        QCOMPARE(o.vars, QStringList({ "Solid", "Normal", "Risky" }));

        QVERIFY(parseOptionLine(QStringLiteral("option name SyzygyPath type string default <empty>"), &o));
        QVERIFY(o.defaultValue.isEmpty());

        QVERIFY(parseOptionLine(QStringLiteral("option name Clear Hash type button"), &o));
        QCOMPARE(o.name, QStringLiteral("Clear Hash"));
        QVERIFY(!parseOptionLine(QStringLiteral("option type spin"), &o));
    }

    void commands()
    {
        GoParameters p;
        p.whiteTime = 60000;
        p.blackTime = 55000;
        p.whiteIncrement = 2000;
        p.blackIncrement = 2000;
        QCOMPARE(goCommand(p), QStringLiteral("go wtime 60000 btime 55000 winc 2000 binc 2000"));
        GoParameters d;
        d.depth = 12;
        QCOMPARE(goCommand(d), QStringLiteral("go depth 12"));
        QCOMPARE(goCommand(GoParameters()), QStringLiteral("go infinite"));

        QCOMPARE(positionCommand(QString(), {}), QStringLiteral("position startpos"));
        QCOMPARE(positionCommand(QString(), { "e2e4", "e7e5" }), QStringLiteral("position startpos moves e2e4 e7e5"));
        QCOMPARE(positionCommand(QStringLiteral("8/8/8/8/8/k7/8/K1Q5 w - - 0 1"), {}),
                 QStringLiteral("position fen 8/8/8/8/8/k7/8/K1Q5 w - - 0 1"));
    }

    void liveStockfish()
    {
        const QString exe = findEngineExecutable(QStringLiteral("stockfish"));
        if (exe.isEmpty())
            QSKIP("Stockfish not installed");

        UciEngine e;
        QSignalSpy ready(&e, &UciEngine::ready);
        QSignalSpy best(&e, &UciEngine::bestMove);
        QSignalSpy infos(&e, &UciEngine::infoReceived);
        e.setOption(QStringLiteral("Threads"), QStringLiteral("1"));
        e.start(exe);
        QVERIFY(ready.wait(15000));
        QVERIFY(e.name().contains(QStringLiteral("Stockfish"), Qt::CaseInsensitive));
        QVERIFY(e.hasOption(QStringLiteral("Threads")));
        QCOMPARE(e.state(), UciEngine::State::Ready);

        // Mate in one must be found
        GoParameters p;
        p.depth = 6;
        const QString fen = QStringLiteral("6k1/5ppp/8/8/8/8/5PPP/R5K1 w - - 0 1");
        e.go(fen, {}, p);
        QVERIFY(best.wait(15000));
        QCOMPARE(best.first().at(0).toString(), QStringLiteral("a1a8"));
        QVERIFY(!infos.isEmpty());

        // A discarded search does not report a move; the next one does.
        best.clear();
        GoParameters inf;
        inf.infinite = true;
        e.go(QString(), { "e2e4" }, inf);
        QTest::qWait(200);
        e.stop(true);
        GoParameters quick;
        quick.depth = 5;
        e.go(QString(), { "e2e4" }, quick);
        QVERIFY(best.wait(15000));
        QTest::qWait(300);
        QCOMPARE(best.size(), 1);
        Position pos = Position::startPosition();
        pos.makeMove(parseUci(pos, QStringLiteral("e2e4")));
        QVERIFY(!parseUci(pos, best.first().at(0).toString()).isNull());
        QCOMPARE(e.state(), UciEngine::State::Ready);

        e.quit();
        QCOMPARE(e.state(), UciEngine::State::NotRunning);
    }

    void xboardFeatures()
    {
        const auto f = parseFeatureLine(QStringLiteral("feature ping=1 setboard=1 myname=\"Crafty-25.2\" option=\"Hash Size -spin 64 1 1024\" done=1"));
        QCOMPARE(f.size(), 5);
        QCOMPARE(f.at(2).first, QStringLiteral("myname"));
        QCOMPARE(f.at(2).second, QStringLiteral("Crafty-25.2"));
        QCOMPARE(f.at(4).second, QStringLiteral("1"));

        EngineOption o;
        QVERIFY(parseXBoardOption(f.at(3).second, &o));
        QCOMPARE(o.name, QStringLiteral("Hash Size"));
        QCOMPARE(o.type, QStringLiteral("spin"));
        QCOMPARE(o.defaultValue, QStringLiteral("64"));
        QCOMPARE(o.max, QStringLiteral("1024"));
        QVERIFY(parseXBoardOption(QStringLiteral("Resign -check 1"), &o));
        QCOMPARE(o.defaultValue, QStringLiteral("true"));
        QVERIFY(parseXBoardOption(QStringLiteral("Style -combo Solid /// *Normal /// Risky"), &o));
        QCOMPARE(o.vars.size(), 3);
        QCOMPARE(o.defaultValue, QStringLiteral("Normal"));
        QVERIFY(!parseXBoardOption(QStringLiteral("nonsense"), &o));
    }

    void xboardPost()
    {
        Position root;
        QVERIFY(root.setFromFen(QStringLiteral("r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2N2N2/PP2BPPP/R2QKB1R b KQ - 0 8")));
        SearchInfo info;
        QVERIFY(parseXBoardPost(QStringLiteral("      15   -100     171 11116445 1. ... Qb6 2. Qb3 Qc7 3. cxd5 Nxd5 <HT>"), root, &info));
        QCOMPARE(info.depth, 15);
        QCOMPARE(info.score, -100);
        QVERIFY(!info.isMate);
        QCOMPARE(info.timeMs, qint64(1710));
        QCOMPARE(info.nodes, qint64(11116445));
        QCOMPARE(info.pv, (QStringList{ "d8b6", "d1b3", "b6c7", "c4d5", "f6d5" }));

        Position mate;
        QVERIFY(mate.setFromFen(QStringLiteral("6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1")));
        QVERIFY(parseXBoardPost(QStringLiteral("       5  32766       0 214 1. Rd8#"), mate, &info));
        QVERIFY(info.isMate);
        QCOMPARE(info.score, 1);
        QCOMPARE(info.pv, QStringList{ "d1d8" });
        QVERIFY(!parseXBoardPost(QStringLiteral("Hint: Qb3"), mate, &info));
        QCOMPARE(xboardMoveToUci(mate, QStringLiteral("Rd8#")), QStringLiteral("d1d8"));
        QCOMPARE(xboardMoveToUci(mate, QStringLiteral("d1d8")), QStringLiteral("d1d8"));
        QVERIFY(xboardMoveToUci(mate, QStringLiteral("Qd8")).isEmpty());
    }

    void liveCrafty()
    {
        const QString exe = qEnvironmentVariable("MBCHESS_CRAFTY");
        if (exe.isEmpty() || !QFileInfo(exe).isExecutable())
            QSKIP("Crafty not available (set MBCHESS_CRAFTY)");
        const EngineInfo info = engineInfo(exe);
        QVERIFY(info.protocol == Protocol::XBoard);
        UciEngine e;
        QSignalSpy known(&e, &UciEngine::optionsKnown);
        QSignalSpy ready(&e, &UciEngine::ready);
        QSignalSpy best(&e, &UciEngine::bestMove);
        QSignalSpy infos(&e, &UciEngine::infoReceived);
        if (qEnvironmentVariableIsSet("MBCHESS_XBDEBUG")) {
            connect(&e, &UciEngine::lineSent, [](const QString &l) { qDebug() << ">" << l; });
            connect(&e, &UciEngine::lineReceived, [](const QString &l) { qDebug() << "<" << l; });
        }
        e.setOption(QStringLiteral("Hash"), QStringLiteral("32"));
        e.start(exe, info.arguments, info.protocol);
        QVERIFY(ready.wait(15000));
        QCOMPARE(known.size(), 1);
        QVERIFY(e.name().startsWith(QStringLiteral("Crafty")));
        QVERIFY(e.hasOption(QStringLiteral("Skill Level")));
        e.setOption(QStringLiteral("Threads"), QStringLiteral("1"));
        e.newGame();

        // mate in one, from a FEN
        GoParameters p;
        p.moveTime = 1000;
        e.go(QStringLiteral("6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1"), {}, p);
        QVERIFY(best.wait(10000));
        QCOMPARE(best.first().at(0).toString(), QStringLiteral("d1d8"));

        // a game from the start: the engine's own move is kept, the next search only
        // sends the new move
        best.clear();
        e.newGame();
        e.go(QString(), { "e2e4" }, p);
        QVERIFY(best.wait(10000));
        const QString reply = best.first().at(0).toString();
        Position pos = Position::startPosition();
        pos.makeMove(parseUci(pos, QStringLiteral("e2e4")));
        QVERIFY(!parseUci(pos, reply).isNull());
        pos.makeMove(parseUci(pos, reply));
        const Move m = pos.legalMoves().front();
        best.clear();
        GoParameters clock;
        clock.whiteTime = clock.blackTime = 60000;
        clock.whiteIncrement = clock.blackIncrement = 1000;
        e.go(QString(), { "e2e4", reply, toUci(m) }, clock);
        QVERIFY(best.wait(20000));
        pos.makeMove(m);
        QVERIFY(!parseUci(pos, best.first().at(0).toString()).isNull());

        // analysis: stop without result, then a new search
        best.clear();
        infos.clear();
        GoParameters inf;
        inf.infinite = true;
        e.go(QStringLiteral("r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2N2N2/PP2BPPP/R2QKB1R b KQ - 0 8"), {}, inf);
        QTest::qWait(1500);
        QVERIFY(!infos.isEmpty());
        e.stop(true);
        QCOMPARE(e.state(), UciEngine::State::Ready);
        e.go(QStringLiteral("6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1"), {}, p);
        QVERIFY(best.wait(10000));
        QCOMPARE(best.size(), 1);
        QCOMPARE(best.first().at(0).toString(), QStringLiteral("d1d8"));

        // cancelled normal search (a position outside the book, so the engine really thinks)
        best.clear();
        GoParameters longer;
        longer.moveTime = 5000;
        e.go(QStringLiteral("r1bq1rk1/pp2bppp/2n1pn2/3p4/2PP4/2N2N2/PP2BPPP/R2QKB1R b KQ - 0 8"), {}, longer);
        QTest::qWait(300);
        e.stop(true);
        e.go(QStringLiteral("6k1/5ppp/8/8/8/8/5PPP/3R2K1 w - - 0 1"), {}, p);
        QVERIFY(best.wait(10000));
        QTest::qWait(500);
        QCOMPARE(best.size(), 1);
        QCOMPARE(best.first().at(0).toString(), QStringLiteral("d1d8"));
        e.quit();
    }

    void missingEngine()
    {
        UciEngine e;
        QSignalSpy err(&e, &UciEngine::errorOccurred);
        e.start(QStringLiteral("/nonexistent/engine"));
        QVERIFY(err.wait(5000) || !err.isEmpty());
        QCOMPARE(e.state(), UciEngine::State::NotRunning);
    }
};

QTEST_GUILESS_MAIN(TestEngine)
#include "tst_engine.moc"
