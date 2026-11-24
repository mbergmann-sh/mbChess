// SPDX-License-Identifier: GPL-3.0-or-later
#include <book/bookbuilder.h>
#include <book/openings.h>
#include <book/polyglot.h>
#include <chesscore/notation.h>

#include <QtTest>

using namespace mbc;
using namespace mbc::book;

class TestBook : public QObject
{
    Q_OBJECT
private slots:
    void keys_data();
    void keys();
    void bookMoves();
    void castlingEncoding();
    void openings();
    void builder();
};

void TestBook::keys_data()
{
    // the example keys from the PolyGlot book format description
    QTest::addColumn<QString>("moves");
    QTest::addColumn<quint64>("key");
    QTest::newRow("start") << "" << Q_UINT64_C(0x463b96181691fc9c);
    QTest::newRow("e4") << "e4" << Q_UINT64_C(0x823c9b50fd114196);
    QTest::newRow("e4 d5") << "e4 d5" << Q_UINT64_C(0x0756b94461c50fb0);
    QTest::newRow("e4 d5 e5") << "e4 d5 e5" << Q_UINT64_C(0x662fafb965db29d4);
    QTest::newRow("e4 d5 e5 f5") << "e4 d5 e5 f5" << Q_UINT64_C(0x22a48b5a8e47ff78);
    QTest::newRow("... Ke2") << "e4 d5 e5 f5 Ke2" << Q_UINT64_C(0x652a607ca3f242c1);
    QTest::newRow("... Kf7") << "e4 d5 e5 f5 Ke2 Kf7" << Q_UINT64_C(0x00fdd303c946bdd9);
    QTest::newRow("a4 b5 h4 b4 c4") << "a4 b5 h4 b4 c4" << Q_UINT64_C(0x3c8123ea7b067637);
    QTest::newRow("... bxc3 Ra3") << "a4 b5 h4 b4 c4 bxc3 Ra3" << Q_UINT64_C(0x5c3f9b829b279560);
}

static Position play(const QString &moves)
{
    Position pos = Position::startPosition();
    for (const QString &san : moves.split(QLatin1Char(' '), Qt::SkipEmptyParts))
        pos.makeMove(parseSan(pos, san));
    return pos;
}

void TestBook::keys()
{
    QFETCH(QString, moves);
    QFETCH(quint64, key);
    QCOMPARE(polyglotKey(play(moves)), key);
}

void TestBook::bookMoves()
{
    const Position start = Position::startPosition();
    const Move e4 = parseSan(start, QStringLiteral("e4"));
    const Move d4 = parseSan(start, QStringLiteral("d4"));
    QByteArray data = PolyglotBook::entry(polyglotKey(start), encodeMove(start, e4), 30)
        + PolyglotBook::entry(polyglotKey(start), encodeMove(start, d4), 10);
    PolyglotBook book;
    QVERIFY(book.setData(data));
    const QVector<BookMove> list = book.moves(start);
    QCOMPARE(list.size(), 2);
    QCOMPARE(list.at(0).move, e4);
    QCOMPARE(list.at(0).weight, 30);
    QCOMPARE(book.pick(start, 0), e4);
    QCOMPARE(book.pick(start, 35), d4);
    QVERIFY(book.moves(play(QStringLiteral("e4"))).isEmpty());
    QVERIFY(!book.setData(QByteArray(15, '\0')));
}

void TestBook::castlingEncoding()
{
    const Position pos = play(QStringLiteral("e4 e5 Nf3 Nc6 Bc4 Bc5"));
    const Move castle = parseSan(pos, QStringLiteral("O-O"));
    // e1h1: from e1 (file 4, rank 0), to h1 (file 7, rank 0)
    QCOMPARE(int(encodeMove(pos, castle)), 7 | 4 << 6);
}

void TestBook::openings()
{
    OpeningIndex index;
    const QString tsv = QStringLiteral(
        "eco\tname\tpgn\n"
        "C60\tRuy Lopez\t1. e4 e5 2. Nf3 Nc6 3. Bb5\n"
        "C65\tRuy Lopez: Berlin Defense\t1. e4 e5 2. Nf3 Nc6 3. Bb5 Nf6\n"
        "XXX\tbroken\t1. e4 e9\n");
    QCOMPARE(index.addTsv(tsv), 2);
    QCOMPARE(index.find(play(QStringLiteral("e4 e5 Nf3 Nc6 Bb5"))).eco, QStringLiteral("C60"));
    // a transposition reaches the same position
    QCOMPARE(index.find(play(QStringLiteral("e4 Nc6 Nf3 e5 Bb5 Nf6"))).name, QStringLiteral("Ruy Lopez: Berlin Defense"));
    QVERIFY(!index.find(Position::startPosition()).isValid());
}

void TestBook::builder()
{
    // e4 wins twice, d4 loses twice, c4 is played once only
    const QByteArray pgn =
        "[Event \"a\"]\n[Result \"1-0\"]\n\n1. e4 {comment} e5 (1... c5 2. Nf3) 2. Nf3 $1 Nc6 1-0\n\n"
        "[Event \"b\"]\n[Result \"1-0\"]\n\n1.e4 e5 2.Nf3 d6 1-0\n\n"
        "[Event \"c\"]\n[Result \"0-1\"]\n\n1. d4 d5 0-1\n"
        "[Event \"d\"]\n[Result \"0-1\"]\n\n1. d4 Nf6 0-1\n"
        "[Event \"e\"]\n[Result \"1/2-1/2\"]\n\n1. c4 e5 1/2-1/2\n";
    BookBuilder builder;
    builder.setMinGames(2);
    builder.addPgnText(pgn);
    QCOMPARE(builder.gameCount(), 5);
    QVERIFY2(builder.positionCount() > 0, "no positions");
    PolyglotBook book;
    QVERIFY(book.setData(builder.polyglotData()));
    const Position start = Position::startPosition();
    const QVector<BookMove> moves = book.moves(start);
    QCOMPARE(moves.size(), 1); // d4 scored badly, c4 too rare
    QCOMPARE(toSan(start, moves.at(0).move), QStringLiteral("e4"));
    QCOMPARE(statsFromLearn(moves.at(0).learn).games, 2);
    QCOMPARE(statsFromLearn(moves.at(0).learn).perMille, 1000);
    // Black's reply after 1. e4: e5 (2 games, lost both) is the only one played twice
    const Position afterE4 = play(QStringLiteral("e4"));
    QVERIFY(book.moves(afterE4).isEmpty()); // scored 0 %: left out
    QCOMPARE(book.moves(play(QStringLiteral("e4 e5"))).size(), 1); // Nf3
}

QTEST_APPLESS_MAIN(TestBook)
#include "tst_book.moc"
