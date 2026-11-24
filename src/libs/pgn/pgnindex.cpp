// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgnindex.h"
#include "pgn.h"

#include <QFile>

#include <cctype>

namespace mbc::pgn {

namespace {

QString decodeValue(const QByteArray &bytes)
{
    const QString s = QString::fromUtf8(bytes);
    if (s.contains(QChar::ReplacementCharacter) && !bytes.contains("\xEF\xBF\xBD"))
        return QString::fromLatin1(bytes);
    return s;
}

// [Name "Value"] -> name, value. Returns false for anything else.
bool parseTag(const QByteArray &line, QByteArray *name, QByteArray *value)
{
    int i = 1;
    while (i < line.size() && line.at(i) == ' ')
        ++i;
    const int nameStart = i;
    while (i < line.size() && line.at(i) != ' ' && line.at(i) != '"' && line.at(i) != ']')
        ++i;
    *name = line.mid(nameStart, i - nameStart);
    while (i < line.size() && line.at(i) != '"')
        ++i;
    if (i >= line.size() || name->isEmpty())
        return false;
    ++i;
    value->clear();
    for (; i < line.size(); ++i) {
        const char c = line.at(i);
        if (c == '\\' && i + 1 < line.size()) {
            value->append(line.at(++i));
        } else if (c == '"') {
            return true;
        } else {
            value->append(c);
        }
    }
    return true; // tolerate a missing closing quote
}

bool isMoveToken(const QByteArray &token)
{
    int i = 0;
    while (i < token.size() && (std::isdigit(static_cast<unsigned char>(token.at(i))) || token.at(i) == '.'))
        ++i;
    if (i >= token.size())
        return false;
    // move numbers like "12." alone are no moves; "1-0", "1/2-1/2" start with a digit and
    // are stripped to "-0" / "/2-1/2"
    const char c = token.at(i);
    return (c >= 'a' && c <= 'h') || c == 'K' || c == 'Q' || c == 'R' || c == 'B' || c == 'N' || c == 'O'
        || c == 'P';
}

} // namespace

bool PgnIndex::scan(const QString &fileName, QString *error, const Progress &progress)
{
    m_fileName = fileName;
    m_text.clear();
    m_games.clear();
    m_inGame = m_inMoves = false;
    m_braceDepth = m_parenDepth = 0;

    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const qint64 size = f.size();
    int lines = 0;
    qint64 pos = 0;
    while (!f.atEnd()) {
        const QByteArray line = f.readLine();
        addLine(line, pos);
        pos += line.size();
        if (progress && ++lines % 8192 == 0 && !progress(pos, size)) {
            if (error)
                error->clear();
            return false;
        }
    }
    finish(pos);
    if (progress)
        progress(size, size);
    return true;
}

void PgnIndex::scanText(const QByteArray &text)
{
    m_fileName.clear();
    m_text = text;
    m_games.clear();
    m_inGame = m_inMoves = false;
    m_braceDepth = m_parenDepth = 0;
    qint64 pos = 0;
    int start = 0;
    while (start < text.size()) {
        int end = text.indexOf('\n', start);
        end = end < 0 ? text.size() : end + 1;
        addLine(text.mid(start, end - start), pos);
        pos += end - start;
        start = end;
    }
    finish(pos);
}

void PgnIndex::addLine(const QByteArray &raw, qint64 pos)
{
    QByteArray line = raw;
    if (pos == 0 && line.startsWith("\xEF\xBB\xBF")) // UTF-8 byte order mark
        line.remove(0, 3);
    line = line.trimmed();
    if (line.isEmpty() || line.startsWith('%'))
        return;

    if (m_braceDepth == 0 && line.startsWith('[')) {
        if (m_inMoves)
            finish(pos);
        if (!m_inGame) {
            m_cur = GameInfo();
            m_cur.offset = pos;
            m_inGame = true;
        }
        QByteArray name, value;
        if (!parseTag(line, &name, &value))
            return;
        const QString v = decodeValue(value).trimmed();
        if (name == "White")
            m_cur.white = v;
        else if (name == "Black")
            m_cur.black = v;
        else if (name == "Event")
            m_cur.event = v;
        else if (name == "Site")
            m_cur.site = v;
        else if (name == "Date")
            m_cur.date = v;
        else if (name == "Round")
            m_cur.round = v;
        else if (name == "Result")
            m_cur.result = v;
        else if (name == "ECO")
            m_cur.eco = v;
        else if (name == "WhiteElo")
            m_cur.whiteElo = v.toInt();
        else if (name == "BlackElo")
            m_cur.blackElo = v.toInt();
        return;
    }

    // movetext
    if (!m_inGame) {
        m_cur = GameInfo();
        m_cur.offset = pos;
        m_inGame = true;
    }
    m_inMoves = true;
    QByteArray token;
    auto endToken = [&]() {
        if (!token.isEmpty() && m_parenDepth == 0 && isMoveToken(token))
            ++m_cur.plies;
        token.clear();
    };
    for (int i = 0; i < line.size(); ++i) {
        const char c = line.at(i);
        if (m_braceDepth > 0) {
            if (c == '}')
                m_braceDepth = 0;
            continue;
        }
        switch (c) {
        case '{':
            endToken();
            m_braceDepth = 1;
            break;
        case ';': // comment up to the end of the line
            endToken();
            return;
        case '(':
            endToken();
            ++m_parenDepth;
            break;
        case ')':
            endToken();
            if (m_parenDepth > 0)
                --m_parenDepth;
            break;
        case ' ':
        case '\t':
            endToken();
            break;
        default:
            token.append(c);
            break;
        }
    }
    endToken();
}

void PgnIndex::finish(qint64 end)
{
    if (m_inGame) {
        m_cur.length = end - m_cur.offset;
        m_games.append(m_cur);
    }
    m_cur = GameInfo();
    m_inGame = m_inMoves = false;
    m_braceDepth = m_parenDepth = 0;
}

bool PgnIndex::load(int i, Game *game, QString *error) const
{
    if (i < 0 || i >= m_games.size()) {
        if (error)
            *error = QStringLiteral("no such game");
        return false;
    }
    const GameInfo &g = m_games.at(i);
    QByteArray bytes;
    if (m_fileName.isEmpty()) {
        bytes = m_text.mid(g.offset, g.length);
    } else {
        QFile f(m_fileName);
        if (!f.open(QIODevice::ReadOnly) || !f.seek(g.offset)) {
            if (error)
                *error = f.errorString();
            return false;
        }
        bytes = f.read(g.length);
    }
    QStringList problems;
    const QVector<Game> games = read(decode(bytes), &problems);
    if (games.isEmpty()) {
        if (error)
            *error = problems.isEmpty() ? QStringLiteral("no game found") : problems.join(QLatin1Char('\n'));
        return false;
    }
    *game = games.first();
    if (error)
        *error = problems.join(QLatin1Char('\n'));
    return true;
}

} // namespace mbc::pgn
