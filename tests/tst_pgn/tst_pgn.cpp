// SPDX-License-Identifier: GPL-3.0-or-later
// Tests for the PGN reader/writer and the Game class.

#include <chesscore/game.h>
#include <chesscore/notation.h>
#include <pgn/pgn.h>
#include <pgn/pgnindex.h>

#include <QtTest>

using namespace mbc;

class TestPgn : public QObject
{
    Q_OBJECT

private:
    static Game playSan(const QStringList &sans)
    {
        Game g;
        for (const QString &s : sans)
            g.addMove(parseSan(g.position(), s));
        return g;
    }

private slots:
    void writeSimpleGame()
    {
        Game g = playSan({ "e4", "e5", "Nf3", "Nc6", "Bb5" });
        g.setTag(QStringLiteral("White"), QStringLiteral("Bergmann, Michael"));
        g.setTag(QStringLiteral("Black"), QStringLiteral("Stockfish"));
        g.setTag(QStringLiteral("Date"), QStringLiteral("2026.11.22"));
        g.setTag(QStringLiteral("TimeControl"), QStringLiteral("300+5"));
        const QString out = pgn::write(g);
        const QString expected = QStringLiteral(
            "[Event \"?\"]\n"
            "[Site \"?\"]\n"
            "[Date \"2026.11.22\"]\n"
            "[Round \"?\"]\n"
            "[White \"Bergmann, Michael\"]\n"
            "[Black \"Stockfish\"]\n"
            "[Result \"*\"]\n"
            "[TimeControl \"300+5\"]\n"
            "\n"
            "1. e4 e5 2. Nf3 Nc6 3. Bb5 *\n\n");
        QCOMPARE(out, expected);
    }

    void writeFromPositionAndWrap()
    {
        Position start;
        QVERIFY(start.setFromFen(QStringLiteral("4k3/8/8/8/8/8/8/R3K3 b Q - 0 30")));
        Game g(start);
        // Black starts: "30... Kd7"
        QVERIFY(g.addMove(parseSan(g.position(), QStringLiteral("Kd7"))));
        QVERIFY(g.addMove(parseSan(g.position(), QStringLiteral("O-O-O+"))));
        g.setResult(Result::Draw);
        const QString out = pgn::write(g);
        QVERIFY(out.contains(QStringLiteral("[SetUp \"1\"]\n[FEN \"4k3/8/8/8/8/8/8/R3K3 b Q - 0 30\"]\n")));
        QVERIFY(out.contains(QStringLiteral("[Result \"1/2-1/2\"]")));
        QVERIFY(out.contains(QStringLiteral("\n30... Kd7 31. O-O-O+ 1/2-1/2\n")));

        // Long games are wrapped below 80 columns
        Game longGame;
        for (int i = 0; i < 20; ++i)
            for (const char *m : { "Nf3", "Nf6", "Ng1", "Ng8" })
                longGame.addMove(parseSan(longGame.position(), QLatin1String(m)));
        for (const QString &line : pgn::write(longGame).split(QLatin1Char('\n')))
            QVERIFY2(line.size() < 80, qPrintable(line));
    }

    void roundTrip()
    {
        Game g = playSan({ "d4", "d5", "c4", "e6", "Nc3", "Nf6", "Bg5", "Be7", "e3", "O-O", "Nf3", "h6" });
        g.setTag(QStringLiteral("Event"), QStringLiteral("Test \"quoted\" \\ event"));
        g.setResult(Result::WhiteWins);
        const QVector<Game> games = pgn::read(pgn::write(g));
        QCOMPARE(games.size(), 1);
        const Game &r = games.first();
        QCOMPARE(r.sanMoves(), g.sanMoves());
        QCOMPARE(r.tag(QStringLiteral("Event")), g.tag(QStringLiteral("Event")));
        QCOMPARE(r.result(), Result::WhiteWins);
    }

    void readComplexMovetext()
    {
        const QString text = QStringLiteral(
            "% escape line ignored\n"
            "[Event \"Casual\"]\n"
            "[White \"A\"]\n"
            "[Black \"B\"]\n"
            "[Result \"0-1\"]\n"
            "\n"
            "1.e4 {best by test} e5 2. Nf3 $1 (2. f4 exf4 (2... d5)) 2... Nc6 ; comment to end of line\n"
            "3.Bc4 Nd4!? 4.Nxe5? Qg5 5.Nxf7 Qxg2 6.Rf1 Qxe4+ 7.Be2 Nf3# 0-1\n"
            "\n"
            "[Event \"Second\"]\n"
            "[SetUp \"1\"]\n"
            "[FEN \"k7/8/1K6/8/8/8/7Q/8 w - - 0 1\"]\n"
            "\n"
            "1. Qh8# 1-0\n");
        QStringList errors;
        const QVector<Game> games = pgn::read(text, &errors);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        QCOMPARE(games.size(), 2);
        QCOMPARE(games.at(0).plyCount(), 14);
        QCOMPARE(games.at(0).san(13), QStringLiteral("Nf3#"));
        QCOMPARE(games.at(0).result(), Result::BlackWins);
        QVERIFY(games.at(0).position().isCheckmate());
        QCOMPARE(games.at(1).tag(QStringLiteral("Event")), QStringLiteral("Second"));
        QCOMPARE(games.at(1).plyCount(), 1);
        QVERIFY(games.at(1).position().isCheckmate());
    }

    void readIllegalMove()
    {
        QStringList errors;
        const QVector<Game> games = pgn::read(QStringLiteral("1. e4 e5 2. Ke3 Nc6 *"), &errors);
        QCOMPARE(games.size(), 1);
        QCOMPARE(games.first().plyCount(), 2);
        QCOMPARE(errors.size(), 1);
    }

    void decodeLatin1AndUtf8()
    {
        const QByteArray utf8 = QStringLiteral("[White \"Müller\"]").toUtf8();
        const QByteArray latin1 = QStringLiteral("[White \"Müller\"]").toLatin1();
        QCOMPARE(pgn::decode(utf8), QStringLiteral("[White \"Müller\"]"));
        QCOMPARE(pgn::decode(latin1), QStringLiteral("[White \"Müller\"]"));
    }

    void threefoldRepetition()
    {
        Game g = playSan({ "Nf3", "Nf6", "Ng1", "Ng8", "Nf3", "Nf6", "Ng1" });
        QCOMPARE(g.status(), GameStatus::Ongoing);
        QVERIFY(g.addMove(parseSan(g.position(), QStringLiteral("Ng8"))));
        QCOMPARE(g.repetitionCount(), 3);
        QCOMPARE(g.status(), GameStatus::ThreefoldRepetition);
        QVERIFY(g.takeBack());
        QCOMPARE(g.status(), GameStatus::Ongoing);
    }

    void gameRejectsIllegalMove()
    {
        Game g;
        QVERIFY(!g.addMove(Move(makeSquare(4, 1), makeSquare(4, 4))));
        QCOMPARE(g.plyCount(), 0);
        QVERIFY(!g.takeBack());
    }

    void gameTreeVariations()
    {
        Game g;
        auto play = [&](const char *san) { QVERIFY(g.addMove(parseSan(g.position(), QLatin1String(san)))); };
        play("e4"); play("e5"); play("Nf3");
        const Game::NodeId nf3 = g.currentNode();
        g.goBack();
        play("Bc4");                       // variation 2. Bc4 instead of 2. Nf3
        const Game::NodeId bc4 = g.currentNode();
        QCOMPARE(g.plyCount(), 3);
        QCOMPARE(g.sanMoves(), (QStringList{ "e4", "e5", "Bc4" }));
        QVERIFY(!g.isMainLine(bc4));
        QVERIFY(g.isMainLine(nf3));
        QCOMPARE(g.mainLine().size(), 3);
        // playing an existing move just follows it
        g.goBack();
        play("Nf3");
        QCOMPARE(g.currentNode(), nf3);
        // promote and delete
        QVERIFY(g.promoteVariation(bc4));
        QVERIFY(g.isMainLine(bc4));
        QVERIFY(g.deleteFrom(nf3));
        QCOMPARE(g.currentNode(), g.parent(bc4)); // cursor was inside the deleted part
        QCOMPARE(g.children(g.currentNode()).size(), 1);
        // navigation
        g.goToStart();
        QCOMPARE(g.plyCount(), 0);
        g.goToLineEnd();
        QCOMPARE(g.sanMoves(), (QStringList{ "e4", "e5", "Bc4" }));
    }

    void variationsRoundTrip()
    {
        const QString text = QStringLiteral(
            "[Event \"Test\"]\n\n"
            "1. e4 {Best by test} e5 (1... c5 2. Nf3 (2. c3) 2... d6) 2. Nf3! {[%eval 0.35] good} Nc6 $5 "
            "3. Bb5 (3. Bc4 Bc5 (3... Nf6) 4. c3) 3... a6 1-0\n");
        QStringList errors;
        const QVector<Game> games = pgn::read(text, &errors);
        QVERIFY2(errors.isEmpty(), qPrintable(errors.join(QLatin1Char('\n'))));
        QCOMPARE(games.size(), 1);
        const Game &g = games.first();
        QCOMPARE(g.sanMoves(), (QStringList{ "e4", "e5", "Nf3", "Nc6", "Bb5", "a6" })); // main line end
        const QVector<Game::NodeId> main = g.mainLine();
        QCOMPARE(g.comment(main.at(0)), QStringLiteral("Best by test"));
        QCOMPARE(g.comment(main.at(2)), QStringLiteral("good"));
        QCOMPARE(g.nags(main.at(2)), QVector<int>{ 1 });
        QCOMPARE(g.nags(main.at(3)), QVector<int>{ 5 });
        QVERIFY(g.evaluation(main.at(2)).valid);
        QCOMPARE(g.evaluation(main.at(2)).value, 35);
        QCOMPARE(g.children(main.at(0)).size(), 2);    // 1... e5 and 1... c5
        const Game::NodeId c5 = g.children(main.at(0)).at(1);
        QCOMPARE(g.nodeSan(c5), QStringLiteral("c5"));
        const Game::NodeId nf3 = g.children(c5).at(0);
        QCOMPARE(g.children(nf3).size(), 1);            // 2... d6
        QCOMPARE(g.children(c5).size(), 2);             // 2. Nf3 and 2. c3

        // write and read again: same tree
        const QString written = pgn::write(g);
        QVERIFY2(written.contains(QStringLiteral("(1... c5 2. Nf3 (2. c3) 2... d6)")), qPrintable(written));
        QVERIFY2(written.contains(QStringLiteral("{[%eval 0.35] good}")), qPrintable(written));
        const QVector<Game> again = pgn::read(written, &errors);
        QVERIFY(errors.isEmpty());
        QCOMPARE(pgn::write(again.first()), written);
    }

    void brokenVariationIsSkipped()
    {
        QStringList errors;
        const QVector<Game> games = pgn::read(QStringLiteral("1. e4 e5 (1... Ke7 2. d4) 2. Nf3 *"), &errors);
        QCOMPARE(errors.size(), 1);
        QCOMPARE(games.first().sanMoves(), (QStringList{ "e4", "e5", "Nf3" }));
    }

    void fileRoundTrip()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString file = dir.filePath(QStringLiteral("games.pgn"));
        Game a = playSan({ "e4", "c5" });
        Game b = playSan({ "d4", "Nf6" });
        QString error;
        QVERIFY(pgn::writeFile(file, a, &error));
        QVERIFY(pgn::appendToFile(file, b, &error));
        QVector<Game> games;
        QVERIFY(pgn::readFile(file, &games, &error));
        QCOMPARE(games.size(), 2);
        QCOMPARE(games.at(1).sanMoves(), b.sanMoves());
    }

    void indexScansTagsAndMoves()
    {
        const QByteArray text =
            "\xEF\xBB\xBF[Event \"Test \\\"A\\\"\"]\n[White \"M\xC3\xBCller\"]\n[Black \"Tal\"]\n[Result \"1-0\"]\n"
            "[WhiteElo \"2100\"]\n[ECO \"C60\"]\n\n"
            "1.e4 e5 {comment (1... c5)} 2. Nf3 (2. f4 exf4 3. Nf3) 2... Nc6 ; rest of line 4. x\n"
            "3. Bb5 a6 1-0\n\n"
            "[Event \"Zwei\"]\n[White \"J\xE4ger\"]\n[Black \"B\"]\n[Result \"1/2-1/2\"]\n\n"
            "1. d4 d5 2. c4 1/2-1/2\n\n"
            "[Event \"Drei\"]\n[Result \"*\"]\n\n*\n";
        mbc::pgn::PgnIndex index;
        index.scanText(text);
        QCOMPARE(index.size(), 3);
        QCOMPARE(index.info(0).event, QStringLiteral("Test \"A\""));
        QCOMPARE(index.info(0).white, QString::fromUtf8("Müller"));
        QCOMPARE(index.info(0).whiteElo, 2100);
        QCOMPARE(index.info(0).eco, QStringLiteral("C60"));
        QCOMPARE(index.info(0).plies, 6);
        QCOMPARE(index.info(1).white, QString::fromUtf8("Jäger")); // Latin-1 in the file
        QCOMPARE(index.info(1).result, QStringLiteral("1/2-1/2"));
        QCOMPARE(index.info(1).plies, 3);
        QCOMPARE(index.info(2).plies, 0);

        mbc::Game g;
        QVERIFY(index.load(1, &g));
        QCOMPARE(g.tag(QStringLiteral("Event")), QStringLiteral("Zwei"));
        QCOMPARE(g.mainLine().size(), 3);
        QVERIFY(index.load(0, &g));
        QCOMPARE(g.mainLine().size(), 6);
        QVERIFY(!index.load(3, &g));
    }

    void indexFile()
    {
        QTemporaryDir dir;
        const QString file = dir.filePath(QStringLiteral("db.pgn"));
        QFile f(file);
        QVERIFY(f.open(QIODevice::WriteOnly));
        for (int i = 0; i < 50; ++i)
            f.write(QStringLiteral("[Event \"G%1\"]\n[Result \"*\"]\n\n1. e4 e5 2. Nf3 *\n\n").arg(i).toUtf8());
        f.close();
        mbc::pgn::PgnIndex index;
        QVERIFY(index.scan(file));
        QCOMPARE(index.size(), 50);
        mbc::Game g;
        QVERIFY(index.load(37, &g));
        QCOMPARE(g.tag(QStringLiteral("Event")), QStringLiteral("G37"));
        QCOMPARE(index.info(37).plies, 3);
    }
};

QTEST_GUILESS_MAIN(TestPgn)
#include "tst_pgn.moc"
