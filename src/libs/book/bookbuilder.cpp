// SPDX-License-Identifier: GPL-3.0-or-later
#include "bookbuilder.h"

#include "polyglot.h"

#include <chesscore/notation.h>

#include <QFile>

#include <algorithm>
#include <cctype>
#include <cmath>

namespace mbc::book {

BookStats statsFromLearn(quint32 learn)
{
    return { int(learn >> 16), int(learn & 0xFFFF) };
}

bool BookBuilder::addPgnFile(const QString &path, QString *error, const std::function<bool(qint64, qint64)> &progress)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const qint64 size = f.size();
    int lines = 0;
    while (!f.atEnd()) {
        addLine(f.readLine());
        if (progress && ++lines % 4096 == 0 && !progress(f.pos(), size))
            return false;
    }
    finishGame();
    if (progress)
        progress(size, size);
    return true;
}

void BookBuilder::addPgnText(const QByteArray &text)
{
    for (const QByteArray &line : text.split('\n'))
        addLine(line);
    finishGame();
}

void BookBuilder::finishGame()
{
    if (m_inMoves || !m_moves.isEmpty()) {
        int points2White = -1;
        if (m_result == "1-0")
            points2White = 2;
        else if (m_result == "0-1")
            points2White = 0;
        else if (m_result == "1/2-1/2")
            points2White = 1;
        if (points2White >= 0 && m_fen.isEmpty()) { // only decided games from the start position
            ++m_games;
            for (int i = 0; i < m_moves.size(); ++i) {
                MoveStat &s = m_stats[m_moves.at(i).first][m_moves.at(i).second];
                ++s.games;
                s.points2 += i % 2 == 0 ? points2White : 2 - points2White;
            }
        }
    }
    m_inMoves = false;
    m_inComment = false;
    m_variationDepth = 0;
    m_fen.clear();
    m_result.clear();
    m_moves.clear();
    m_pos = Position::startPosition();
    m_broken = false;
}

void BookBuilder::addLine(const QByteArray &raw)
{
    const QByteArray line = raw.trimmed();
    if (!m_inComment && line.startsWith('[')) {
        if (m_inMoves)
            finishGame(); // a game without result token
        if (line.startsWith("[Result ")) {
            const int a = line.indexOf('"'), b = line.lastIndexOf('"');
            if (a >= 0 && b > a)
                m_result = line.mid(a + 1, b - a - 1);
        } else if (line.startsWith("[FEN ")) {
            m_fen = QStringLiteral("x");
        }
        return;
    }
    if (line.isEmpty() || line.startsWith('%'))
        return;
    m_inMoves = true;
    if (m_moves.size() >= m_maxPly && !m_inComment && m_variationDepth == 0) {
        // enough moves: only look for the end of the game
        if (line.endsWith("1-0") || line.endsWith("0-1") || line.endsWith("1/2-1/2") || line.endsWith('*'))
            finishGame();
        return;
    }
    int i = 0;
    const int n = line.size();
    while (i < n) {
        const char c = line.at(i);
        if (m_inComment) {
            if (c == '}')
                m_inComment = false;
            ++i;
            continue;
        }
        if (c == '{') { m_inComment = true; ++i; continue; }
        if (c == ';') break; // comment to the end of the line
        if (c == '(') { ++m_variationDepth; ++i; continue; }
        if (c == ')') { if (m_variationDepth > 0) --m_variationDepth; ++i; continue; }
        if (c == ' ' || c == '\t' || c == '\r') { ++i; continue; }
        int j = i;
        while (j < n && line.at(j) != ' ' && line.at(j) != '{' && line.at(j) != '(' && line.at(j) != ')'
               && line.at(j) != ';' && line.at(j) != '\t')
            ++j;
        QByteArray token = line.mid(i, j - i);
        i = j;
        if (m_variationDepth > 0)
            continue;
        if (token == "1-0" || token == "0-1" || token == "1/2-1/2" || token == "*") {
            if (m_result.isEmpty())
                m_result = token;
            finishGame();
            continue;
        }
        if (token.startsWith('$'))
            continue;
        // "12." "12..." "12.e4"
        int k = 0;
        while (k < token.size() && (std::isdigit(uchar(token.at(k))) || token.at(k) == '.'))
            ++k;
        if (k > 0 && k < token.size() && token.at(k - 1) == '.')
            token = token.mid(k);
        else if (k == token.size())
            continue;
        if (m_broken || m_moves.size() >= m_maxPly)
            continue;
        const Move m = parseSan(m_pos, QString::fromLatin1(token));
        if (m.isNull()) {
            m_broken = true; // keep what was read so far
            continue;
        }
        m_moves.append({ polyglotKey(m_pos), encodeMove(m_pos, m) });
        m_pos.makeMove(m);
    }
}

QByteArray BookBuilder::polyglotData() const
{
    struct Entry { quint64 key; quint16 move; quint16 weight; quint32 learn; };
    QVector<Entry> entries;
    for (auto pos = m_stats.constBegin(); pos != m_stats.constEnd(); ++pos) {
        // the best expected score among the moves played often enough
        double best = 0;
        for (auto mv = pos.value().constBegin(); mv != pos.value().constEnd(); ++mv)
            if (mv.value().games >= m_minGames)
                best = std::max(best, (mv.value().points2 / 2.0 + 1) / (mv.value().games + 2));
        for (auto mv = pos.value().constBegin(); mv != pos.value().constEnd(); ++mv) {
            const MoveStat &s = mv.value();
            if (s.games < m_minGames)
                continue;
            // expected score with a little caution for few games (Laplace)
            const double score = (s.points2 / 2.0 + 1) / (s.games + 2);
            if (score < 0.35 || score < best - 0.15)
                continue; // played, but did badly
            const double w = std::sqrt(double(s.games)) * std::pow(score, 4) * 1000;
            const quint32 perMille = quint32(std::lround(s.points2 * 500.0 / s.games));
            entries.append({ pos.key(), mv.key(), quint16(qBound(1.0, w, 65535.0)),
                             quint32(qMin(s.games, 65535)) << 16 | perMille });
        }
    }
    std::sort(entries.begin(), entries.end(), [](const Entry &a, const Entry &b) {
        return a.key != b.key ? a.key < b.key : a.weight > b.weight;
    });
    QByteArray data;
    data.reserve(entries.size() * 16);
    for (const Entry &e : entries)
        data += PolyglotBook::entry(e.key, e.move, e.weight, e.learn);
    return data;
}

} // namespace mbc::book
