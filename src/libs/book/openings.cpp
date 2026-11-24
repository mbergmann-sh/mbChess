// SPDX-License-Identifier: GPL-3.0-or-later
#include "openings.h"

#include "polyglot.h"

#include <chesscore/notation.h>

#include <QStringList>

namespace mbc::book {

int OpeningIndex::addTsv(const QString &tsv)
{
    int count = 0;
    const QStringList lines = tsv.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList cols = line.trimmed().split(QLatin1Char('\t'));
        if (cols.size() < 3 || cols.at(0) == QLatin1String("eco"))
            continue;
        Position pos = Position::startPosition();
        bool ok = true;
        const QStringList tokens = cols.at(2).split(QLatin1Char(' '), Qt::SkipEmptyParts);
        for (const QString &t : tokens) {
            if (t.endsWith(QLatin1Char('.')))
                continue; // move number
            const Move m = parseSan(pos, t);
            if (m.isNull()) {
                ok = false;
                break;
            }
            pos.makeMove(m);
        }
        if (!ok)
            continue;
        const quint64 key = polyglotKey(pos);
        if (!m_byKey.contains(key)) // the first (and usually more general) name wins
            m_byKey.insert(key, { cols.at(0), cols.at(1) });
        ++count;
    }
    return count;
}

Opening OpeningIndex::find(const Position &pos) const
{
    return m_byKey.value(polyglotKey(pos));
}

} // namespace mbc::book
