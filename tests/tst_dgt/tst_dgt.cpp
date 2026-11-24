// SPDX-License-Identifier: GPL-3.0-or-later
// Tests of the DGT protocol part (no board needed).
#include <dgt/dgtprotocol.h>

#include <QtTest>

using namespace mbc;
using namespace mbc::dgt;

class TestDgt : public QObject
{
    Q_OBJECT
private slots:
    void fields()
    {
        QCOMPARE(squareFromField(0), makeSquare(0, 7));  // a8
        QCOMPARE(squareFromField(63), makeSquare(7, 0)); // h1
        QCOMPARE(squareFromField(0, true), makeSquare(7, 0));
        for (int f = 0; f < 64; ++f) {
            QCOMPARE(fieldFromSquare(squareFromField(f)), f);
            QCOMPARE(fieldFromSquare(squareFromField(f, true), true), f);
        }
    }

    void pieces()
    {
        QCOMPARE(pieceFromCode(0), NoPiece);
        QCOMPARE(pieceFromCode(1), WhitePawn);
        QCOMPARE(pieceFromCode(2), WhiteRook);
        QCOMPARE(pieceFromCode(5), WhiteKing);
        QCOMPARE(pieceFromCode(6), WhiteQueen);
        QCOMPARE(pieceFromCode(9), BlackKnight);
        QCOMPARE(pieceFromCode(12), BlackQueen);
        QCOMPARE(pieceFromCode(13), NoPiece);
        for (int c = 0; c <= 12; ++c)
            QCOMPARE(codeFromPiece(pieceFromCode(c)), c);
    }

    void receiver()
    {
        // board dump of the initial position, split into odd packets as over Bluetooth
        QByteArray dump;
        dump += char(MsgBoardDump);
        dump += char(0);
        dump += char(67);
        for (int f = 0; f < 64; ++f)
            dump += char(codeFromPiece(initialBoard().at(squareFromField(f))));
        QByteArray stream = QByteArray("\x01\x02", 2) + dump; // noise first
        stream += char(MsgFieldUpdate);
        stream += char(0);
        stream += char(5);
        stream += char(52); // e2
        stream += char(0);  // empty
        Receiver r;
        QVector<Message> all;
        for (int i = 0; i < stream.size(); i += 7)
            all += r.feed(stream.mid(i, 7));
        QCOMPARE(all.size(), 2);
        QCOMPARE(all.at(0).id, int(MsgBoardDump));
        QCOMPARE(all.at(0).data.size(), 64);
        QCOMPARE(placementFen(boardFromDump(all.at(0).data)), QStringLiteral("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR"));
        QCOMPARE(placementFen(boardFromDump(all.at(0).data, true)), QStringLiteral("RNBKQBNR/PPPPPPPP/8/8/8/8/pppppppp/rnbkqbnr"));
        QCOMPARE(all.at(1).id, int(MsgFieldUpdate));
        QCOMPARE(int(all.at(1).data.at(0)), 52);
        QCOMPARE(squareFromField(52), makeSquare(4, 1)); // e2
    }
};

QTEST_GUILESS_MAIN(TestDgt)
#include "tst_dgt.moc"
