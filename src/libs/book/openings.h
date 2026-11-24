// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - names of openings (ECO code and name), recognised by position, so that
// transpositions are found too. Data: lichess chess-openings (CC0), one TSV line per
// opening: "eco<TAB>name<TAB>1. e4 e5 2. Nf3 ...".
#pragma once

#include <chesscore/position.h>

#include <QHash>
#include <QString>

namespace mbc::book {

struct Opening
{
    QString eco;    // e.g. "C65"
    QString name;   // e.g. "Ruy Lopez: Berlin Defense"
    bool isValid() const { return !eco.isEmpty(); }
};

class OpeningIndex
{
public:
    // Adds the openings of a TSV text (header line allowed). Returns how many were read.
    int addTsv(const QString &tsv);
    int size() const { return int(m_byKey.size()); }
    // The opening that ends in exactly this position (invalid if none).
    Opening find(const Position &pos) const;

private:
    QHash<quint64, Opening> m_byKey;
};

} // namespace mbc::book
