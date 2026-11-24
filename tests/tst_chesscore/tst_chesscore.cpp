// SPDX-License-Identifier: GPL-3.0-or-later
// Tests for chesscore: perft (move generator), FEN, SAN, UCI, game status.

#include <chesscore/notation.h>
#include <chesscore/position.h>

#include <QtTest>

using namespace mbc;

class TestChessCore : public QObject
{
    Q_OBJECT

private:
    static Position fromFen(const char *fen)
    {
        Position p;
        QString error;
        const bool ok = p.setFromFen(QString::fromLatin1(fen), &error);
        if (!ok)
            qWarning("FEN rejected: %s (%s)", fen, qPrintable(error));
        return p;
    }

    // Plays a sequence of SAN moves and returns the final position.
    static Position play(const QStringList &sans, bool *ok)
    {
        Position p = Position::startPosition();
        *ok = true;
        for (const QString &s : sans) {
            const Move m = parseSan(p, s);
            if (m.isNull()) {
                *ok = false;
                return p;
            }
            p.makeMove(m);
        }
        return p;
    }

private slots:
    // --- Perft: reference values from the Chess Programming Wiki ---
    void perft_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<int>("depth");
        QTest::addColumn<quint64>("nodes");

        const QString start = QString::fromLatin1(Position::StartFen);
        QTest::newRow("start d1") << start << 1 << quint64(20);
        QTest::newRow("start d2") << start << 2 << quint64(400);
        QTest::newRow("start d3") << start << 3 << quint64(8902);
        QTest::newRow("start d4") << start << 4 << quint64(197281);

        const QString kiwipete = QStringLiteral("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        QTest::newRow("kiwipete d1") << kiwipete << 1 << quint64(48);
        QTest::newRow("kiwipete d2") << kiwipete << 2 << quint64(2039);
        QTest::newRow("kiwipete d3") << kiwipete << 3 << quint64(97862);

        const QString pos3 = QStringLiteral("8/2p5/3p4/KP5r/1R3p1k/8/4P1P1/8 w - - 0 1");
        QTest::newRow("pos3 d1") << pos3 << 1 << quint64(14);
        QTest::newRow("pos3 d2") << pos3 << 2 << quint64(191);
        QTest::newRow("pos3 d3") << pos3 << 3 << quint64(2812);
        QTest::newRow("pos3 d4") << pos3 << 4 << quint64(43238);
        QTest::newRow("pos3 d5") << pos3 << 5 << quint64(674624);

        const QString pos4 = QStringLiteral("r3k2r/Pppp1ppp/1b3nbN/nP6/BBP1P3/q4N2/Pp1P2PP/R2Q1RK1 w kq - 0 1");
        QTest::newRow("pos4 d1") << pos4 << 1 << quint64(6);
        QTest::newRow("pos4 d2") << pos4 << 2 << quint64(264);
        QTest::newRow("pos4 d3") << pos4 << 3 << quint64(9467);

        const QString pos5 = QStringLiteral("rnbq1k1r/pp1Pbppp/2p5/8/2B5/8/PPP1NnPP/RNBQK2R w KQ - 1 8");
        QTest::newRow("pos5 d1") << pos5 << 1 << quint64(44);
        QTest::newRow("pos5 d2") << pos5 << 2 << quint64(1486);
        QTest::newRow("pos5 d3") << pos5 << 3 << quint64(62379);

        const QString pos6 = QStringLiteral("r4rk1/1pp1qppp/p1np1n2/2b1p1B1/2B1P1b1/P1NP1N2/1PP1QPPP/R4RK1 w - - 0 10");
        QTest::newRow("pos6 d1") << pos6 << 1 << quint64(46);
        QTest::newRow("pos6 d2") << pos6 << 2 << quint64(2079);
        QTest::newRow("pos6 d3") << pos6 << 3 << quint64(89890);
    }

    void perft()
    {
        QFETCH(QString, fen);
        QFETCH(int, depth);
        QFETCH(quint64, nodes);
        Position p;
        QVERIFY(p.setFromFen(fen));
        QCOMPARE(quint64(mbc::perft(p, depth)), nodes);
    }

    // --- FEN ---
    void fenRoundTrip_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::newRow("start") << QString::fromLatin1(Position::StartFen);
        QTest::newRow("kiwipete") << QStringLiteral("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        QTest::newRow("ep") << QStringLiteral("rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3");
        QTest::newRow("black") << QStringLiteral("8/8/8/8/8/8/k7/4K3 b - - 12 40");
    }

    void fenRoundTrip()
    {
        QFETCH(QString, fen);
        Position p;
        QVERIFY(p.setFromFen(fen));
        QCOMPARE(p.toFen(), fen);
    }

    void fenFourFields()
    {
        Position p;
        QVERIFY(p.setFromFen(QStringLiteral("8/8/8/8/8/8/k7/4K3 w - -")));
        QCOMPARE(p.toFen(), QStringLiteral("8/8/8/8/8/8/k7/4K3 w - - 0 1"));
    }

    void fenInvalid_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::newRow("empty") << QString();
        QTest::newRow("7 ranks") << QStringLiteral("8/8/8/8/8/8/K6k w - - 0 1");
        QTest::newRow("no black king") << QStringLiteral("8/8/8/8/8/8/8/K7 w - - 0 1");
        QTest::newRow("pawn on rank 8") << QStringLiteral("P6k/8/8/8/8/8/8/K7 w - - 0 1");
        QTest::newRow("bad side") << QStringLiteral("7k/8/8/8/8/8/8/K7 x - - 0 1");
        QTest::newRow("bad piece") << QStringLiteral("7k/8/8/8/8/8/8/K6X w - - 0 1");
        QTest::newRow("long rank") << QStringLiteral("7k/9/8/8/8/8/8/K7 w - - 0 1");
        QTest::newRow("bad ep") << QStringLiteral("7k/8/8/8/8/8/8/K7 w - e4 0 1");
        QTest::newRow("opponent in check") << QStringLiteral("k7/8/8/8/8/8/8/R3K3 w - - 0 1");
    }

    void fenInvalid()
    {
        QFETCH(QString, fen);
        Position p = Position::startPosition();
        QString error;
        QVERIFY(!p.setFromFen(fen, &error));
        QVERIFY(!error.isEmpty());
        QCOMPARE(p.toFen(), QString::fromLatin1(Position::StartFen)); // unchanged
    }

    void fenDropsImpossibleCastling()
    {
        Position p;
        QVERIFY(p.setFromFen(QStringLiteral("r3k3/8/8/8/8/8/8/4K2R w KQkq - 0 1")));
        QCOMPARE(p.toFen(), QStringLiteral("r3k3/8/8/8/8/8/8/4K2R w Kq - 0 1"));
    }

    // --- SAN ---
    void sanOutput_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<QString>("uci");
        QTest::addColumn<QString>("san");

        const QString start = QString::fromLatin1(Position::StartFen);
        QTest::newRow("pawn push") << start << "e2e4" << "e4";
        QTest::newRow("knight") << start << "g1f3" << "Nf3";
        QTest::newRow("file disambiguation")
            << "7k/8/8/8/8/2N3N1/8/4K3 w - - 0 1" << "c3e4" << "Nce4";
        QTest::newRow("rank disambiguation")
            << "7k/8/8/N7/8/8/8/N3K3 w - - 0 1" << "a1b3" << "N1b3";
        QTest::newRow("square disambiguation")
            << "8/2k5/8/8/4Q2Q/8/8/K6Q w - - 0 1" << "h4e1" << "Qh4e1";
        QTest::newRow("pawn capture")
            << "rnbqkbnr/ppp1pppp/8/3p4/4P3/8/PPPP1PPP/RNBQKBNR w KQkq d6 0 2" << "e4d5" << "exd5";
        QTest::newRow("en passant")
            << "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3" << "e5f6" << "exf6";
        QTest::newRow("castle short")
            << "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" << "e1g1" << "O-O";
        QTest::newRow("castle long")
            << "r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1" << "e1c1" << "O-O-O";
        QTest::newRow("promotion check")
            << "4k3/1P6/8/8/8/8/8/4K3 w - - 0 1" << "b7b8q" << "b8=Q+";
        QTest::newRow("underpromotion capture")
            << "2r1k3/1P6/8/8/8/8/8/4K3 w - - 0 1" << "b7c8n" << "bxc8=N";
        QTest::newRow("mate")
            << "r1bqkb1r/pppp1ppp/2n2n2/4p2Q/2B1P3/8/PPPP1PPP/RNB1K1NR w KQkq - 4 4" << "h5f7" << "Qxf7#";
    }

    void sanOutput()
    {
        QFETCH(QString, fen);
        QFETCH(QString, uci);
        QFETCH(QString, san);
        Position p;
        QVERIFY(p.setFromFen(fen));
        const Move m = parseUci(p, uci);
        QVERIFY(!m.isNull());
        QCOMPARE(toSan(p, m), san);
        QCOMPARE(parseSan(p, san), m); // and back
    }

    void sanLenientInput_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<QString>("input");
        QTest::addColumn<QString>("uci");

        const QString start = QString::fromLatin1(Position::StartFen);
        const QString kiwi = QStringLiteral("r3k2r/p1ppqpb1/bn2pnp1/3PN3/1p2P3/2N2Q1p/PPPBBPPP/R3K2R w KQkq - 0 1");
        const QString promo = QStringLiteral("4k3/1P6/8/8/8/8/8/4K3 w - - 0 1");
        QTest::newRow("zero castle") << kiwi << "0-0" << "e1g1";
        QTest::newRow("zero castle long") << kiwi << "0-0-0" << "e1c1";
        QTest::newRow("annotation") << start << "e4!?" << "e2e4";
        QTest::newRow("long algebraic") << start << "Ng1-f3" << "g1f3";
        QTest::newRow("pawn long") << start << "e2e4" << "e2e4";
        QTest::newRow("promotion without =") << promo << "b8Q" << "b7b8q";
        QTest::newRow("lowercase promotion") << promo << "b8=n" << "b7b8n";
        QTest::newRow("ep suffix")
            << "rnbqkbnr/ppp1p1pp/8/3pPp2/8/8/PPPP1PPP/RNBQKBNR w KQkq f6 0 3" << "exf6 e.p." << "e5f6";
    }

    void sanLenientInput()
    {
        QFETCH(QString, fen);
        QFETCH(QString, input);
        QFETCH(QString, uci);
        Position p;
        QVERIFY(p.setFromFen(fen));
        const Move m = parseSan(p, input);
        QVERIFY2(!m.isNull(), qPrintable(input));
        QCOMPARE(toUci(m), uci);
    }

    void sanRejects_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<QString>("input");
        const QString start = QString::fromLatin1(Position::StartFen);
        QTest::newRow("illegal") << start << "e5";
        QTest::newRow("ambiguous") << "7k/8/8/8/8/2N3N1/8/4K3 w - - 0 1" << "Ne4";
        QTest::newRow("missing promotion") << "4k3/1P6/8/8/8/8/8/4K3 w - - 0 1" << "b8";
        QTest::newRow("castle not allowed") << start << "O-O";
        QTest::newRow("garbage") << start << "hello";
        QTest::newRow("empty") << start << "";
    }

    void sanRejects()
    {
        QFETCH(QString, fen);
        QFETCH(QString, input);
        Position p;
        QVERIFY(p.setFromFen(fen));
        QVERIFY(parseSan(p, input).isNull());
    }

    void localizedSan()
    {
        QCOMPARE(localizeSan(QStringLiteral("Nf3"), QStringLiteral("KDTLS")), QStringLiteral("Sf3"));
        QCOMPARE(localizeSan(QStringLiteral("exd8=Q+"), QStringLiteral("KDTLS")), QStringLiteral("exd8=D+"));
        QCOMPARE(localizeSan(QStringLiteral("Bxb5"), QStringLiteral("KDTLS")), QStringLiteral("Lxb5"));
        QCOMPARE(localizeSan(QStringLiteral("O-O"), QStringLiteral("KDTLS")), QStringLiteral("O-O"));
    }

    // --- Game play and status ---
    void scholarsMate()
    {
        bool ok = false;
        const Position p = play({ "e4", "e5", "Qh5", "Nc6", "Bc4", "Nf6", "Qxf7#" }, &ok);
        QVERIFY(ok);
        QVERIFY(p.isCheckmate());
        QCOMPARE(p.status(), GameStatus::Checkmate);
        QCOMPARE(p.sideToMove(), Black);
        QCOMPARE(p.fullmoveNumber(), 4);
    }

    void castlingRightsUpdate()
    {
        bool ok = false;
        const Position p = play({ "e4", "e5", "Nf3", "Nc6", "Rg1", "Rb8" }, &ok);
        QVERIFY(ok);
        QCOMPARE(p.toFen(), QStringLiteral("1rbqkbnr/pppp1ppp/2n5/4p3/4P3/5N2/PPPP1PPP/RNBQKBR1 w Qk - 4 4"));
    }

    void doublePushSetsEnPassant()
    {
        Position p = Position::startPosition();
        p.makeMove(parseSan(p, QStringLiteral("e4")));
        QCOMPARE(p.toFen(), QStringLiteral("rnbqkbnr/pppppppp/8/8/4P3/8/PPPP1PPP/RNBQKBNR b KQkq e3 0 1"));
    }

    void stalemate()
    {
        const Position p = fromFen("7k/5Q2/6K1/8/8/8/8/8 b - - 0 1");
        QVERIFY(p.isStalemate());
        QCOMPARE(p.status(), GameStatus::Stalemate);
    }

    void insufficientMaterial_data()
    {
        QTest::addColumn<QString>("fen");
        QTest::addColumn<bool>("draw");
        QTest::newRow("K vs K") << "8/8/8/4k3/8/8/8/4K3 w - - 0 1" << true;
        QTest::newRow("KN vs K") << "8/8/8/4k3/8/8/8/4KN2 w - - 0 1" << true;
        QTest::newRow("KB vs K") << "8/8/8/4k3/8/8/8/4KB2 w - - 0 1" << true;
        QTest::newRow("KB vs KB same colour") << "8/8/8/4k3/8/8/8/3bKB2 w - - 0 1" << true;
        QTest::newRow("KB vs KB other colour") << "8/8/8/4k3/8/8/8/2b1KB2 w - - 0 1" << false;
        QTest::newRow("KNN vs K") << "8/8/8/4k3/8/8/8/3NKN2 w - - 0 1" << false;
        QTest::newRow("KP vs K") << "8/8/8/4k3/8/8/4P3/4K3 w - - 0 1" << false;
    }

    void insufficientMaterial()
    {
        QFETCH(QString, fen);
        QFETCH(bool, draw);
        Position p;
        QVERIFY(p.setFromFen(fen));
        QCOMPARE(p.isInsufficientMaterial(), draw);
    }

    void hashTransposition()
    {
        bool ok1 = false, ok2 = false;
        const Position a = play({ "Nf3", "Nf6", "Nc3", "Nc6" }, &ok1);
        const Position b = play({ "Nc3", "Nc6", "Nf3", "Nf6" }, &ok2);
        QVERIFY(ok1 && ok2);
        QCOMPARE(a.hash(), b.hash());
        QVERIFY(a.hash() != Position::startPosition().hash());
    }

    void editPosition()
    {
        Position p; // empty board: no king, inCheck() must not crash
        QVERIFY(!p.inCheck());
        QVERIFY(!p.isValid());
        p.setPiece(4, WhiteKing);
        p.setPiece(7, WhiteRook);
        p.setPiece(60, BlackKing);
        p.setCastlingRights(WhiteKingSide | WhiteQueenSide | BlackKingSide);
        QCOMPARE(p.castlingRights(), int(WhiteKingSide)); // only what the pieces allow
        QVERIFY(p.isValid());
        QCOMPARE(p.toFen(), QStringLiteral("4k3/8/8/8/8/8/8/4K2R w K - 0 1"));
        p.setPiece(7, NoPiece);
        QCOMPARE(p.castlingRights(), int(NoCastling));
        // en passant only behind a pawn that just made a double step
        p.setPiece(36, BlackPawn); // e5
        p.setEnPassantSquare(44);  // e6
        QCOMPARE(p.enPassantSquare(), 44);
        p.setEnPassantSquare(43);  // d6: no pawn on d5
        QCOMPARE(p.enPassantSquare(), int(NoSquare));
        // the side not to move must not be in check
        p.setPiece(52, WhiteQueen); // e7 next to the black king, White to move
        QVERIFY(!p.isValid());
        p.setSideToMove(Black);
        QVERIFY(p.isValid());
        QVERIFY(p.inCheck());
    }
};

QTEST_GUILESS_MAIN(TestChessCore)
#include "tst_chesscore.moc"
