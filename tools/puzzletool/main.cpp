// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess puzzle tool: reads mate puzzles (one per line, the mbChess format of
// src/app/resources/training/*.txt: id;fen;opponent move or -;solution;rating;themes)
// from stdin and writes for each: id, OK/BAD, mate distance, number of key moves, nodes,
// time, the key moves and a main line (UCI).
#include <chesscore/matesolver.h>
#include <chesscore/notation.h>
#include <chesscore/position.h>

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QTextStream>

#include <cstdio>

using namespace mbc;

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const int maxN = argc > 1 ? QString::fromLatin1(argv[1]).toInt() : 4;
    QTextStream in(stdin);
    QTextStream out(stdout);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const QStringList f = line.split(QLatin1Char(';'));
        if (f.size() < 4) {
            out << "? BAD format " << line << Qt::endl;
            continue;
        }
        Position pos;
        if (!pos.setFromFen(f[1])) {
            out << f[0] << " BAD fen" << Qt::endl;
            continue;
        }
        if (f[2] != QLatin1String("-")) {
            const Move m = parseUci(pos, f[2]);
            if (m.isNull()) {
                out << f[0] << " BAD first" << Qt::endl;
                continue;
            }
            pos.makeMove(m);
        }
        QElapsedTimer t;
        t.start();
        MateSolver solver;
        const int d = solver.mateDistance(pos, maxN);
        const MoveList keyList = d > 0 ? solver.keyMoves(pos, d) : MoveList();
        const int keys = int(keyList.size());
        QStringList keySan;
        for (const Move k : keyList)
            keySan << toSan(pos, k);
        // a main line: shortest mate against the most stubborn defence
        QStringList mainLine;
        {
            Position q = pos;
            for (int left = d; left > 0;) {
                int l = 0;
                const Move a = solver.bestAttack(q, left, &l);
                if (a.isNull())
                    break;
                mainLine << toUci(a);
                q.makeMove(a);
                if (q.isCheckmate())
                    break;
                const Move r = solver.bestDefence(q, l - 1, &left);
                if (r.isNull())
                    break;
                mainLine << toUci(r);
                q.makeMove(r);
            }
        }
        // the solution line must be legal and end in mate
        Position p = pos;
        bool lineOk = true;
        for (const QString &u : f[3].split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
            const Move m = parseUci(p, u);
            if (m.isNull() || !p.isLegal(m)) {
                lineOk = false;
                break;
            }
            p.makeMove(m);
        }
        lineOk = lineOk && p.isCheckmate();
        out << f[0] << (d > 0 && lineOk ? " OK " : " BAD ") << d << ' ' << keys << ' ' << solver.nodes() << ' '
            << t.elapsed() << "ms " << keySan.join(QLatin1Char(',')) << " line:" << mainLine.join(QLatin1Char(' '))
            << Qt::endl;
    }
    return 0;
}
