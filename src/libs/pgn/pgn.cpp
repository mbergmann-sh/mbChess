// SPDX-License-Identifier: GPL-3.0-or-later
#include "pgn.h"

#include <chesscore/notation.h>

#include <QFile>
#include <QRegularExpression>

namespace mbc::pgn {

namespace {

const char *const kSevenTagRoster[] = { "Event", "Site", "Date", "Round", "White", "Black", "Result" };

QString escapeTagValue(QString v)
{
    v.replace(QLatin1Char('\\'), QLatin1String("\\\\"));
    v.replace(QLatin1Char('"'), QLatin1String("\\\""));
    return v;
}

QString defaultTagValue(const QString &name)
{
    if (name == QLatin1String("Date"))
        return QStringLiteral("????.??.??");
    return QStringLiteral("?");
}

bool isResultToken(const QString &t)
{
    return t == QLatin1String("1-0") || t == QLatin1String("0-1") || t == QLatin1String("1/2-1/2")
        || t == QLatin1String("*");
}

bool isCastlingToken(const QString &t)
{
    QString s = t;
    while (!s.isEmpty() && QStringLiteral("+#!?").contains(s.back()))
        s.chop(1);
    s.replace(QLatin1Char('0'), QLatin1Char('O'));
    return s == QLatin1String("O-O") || s == QLatin1String("O-O-O");
}

// Parser state for one game
struct GameBuilder
{
    Game game;
    bool hasContent = false;     // tags or moves seen
    bool started = false;        // start position set up (after the tag section)
    bool broken = false;         // invalid FEN: ignore the movetext
    int brokenDepth = -1;        // illegal move in a variation at this depth: skip it
    QVector<Game::NodeId> stack; // cursor before each open variation
    QMap<QString, QString> tags;

    void begin(QStringList *errors, int number)
    {
        if (started)
            return;
        started = true;
        Position start = Position::startPosition();
        const QString fen = tags.value(QStringLiteral("FEN"));
        if (!fen.isEmpty()) {
            QString err;
            if (!start.setFromFen(fen, &err)) {
                if (errors)
                    *errors << QStringLiteral("Game %1: invalid FEN (%2)").arg(number).arg(err);
                broken = true;
                start = Position::startPosition();
            }
        }
        game.reset(start);
        for (auto it = tags.constBegin(); it != tags.constEnd(); ++it)
            if (it.key() != QLatin1String("FEN") && it.key() != QLatin1String("SetUp"))
                game.setTag(it.key(), it.value());
        game.setResult(resultFromString(tags.value(QStringLiteral("Result"))));
    }
};


QString commentToken(const QString &text, const Evaluation &eval)
{
    QString c = text;
    c.replace(QLatin1Char('}'), QLatin1Char(')'));
    if (eval.valid)
        c = QStringLiteral("[%eval %1]").arg(eval.mate ? eval.toString() : QString::number(eval.value / 100.0, 'f', 2))
            + (c.isEmpty() ? QString() : QStringLiteral(" ") + c);
    return QStringLiteral("{%1}").arg(c);
}

// Tokens for one move: number (if needed), SAN, NAGs, comment
void writeMove(const Game &game, Game::NodeId id, bool forceNumber, QStringList *tokens)
{
    const Position &before = game.nodePosition(game.parent(id));
    const int number = before.fullmoveNumber();
    if (before.sideToMove() == White)
        *tokens << QStringLiteral("%1.").arg(number);
    else if (forceNumber)
        *tokens << QStringLiteral("%1...").arg(number);
    *tokens << game.nodeSan(id);
    for (const int nag : game.nags(id))
        *tokens << QStringLiteral("$%1").arg(nag);
    if (!game.comment(id).isEmpty() || game.evaluation(id).valid)
        *tokens << commentToken(game.comment(id), game.evaluation(id));
}

// Writes the main continuation from "from" on, with the variations at every move.
void writeContinuation(const Game &game, Game::NodeId from, bool forceNumber, QStringList *tokens)
{
    Game::NodeId current = from;
    bool force = forceNumber;
    while (!game.children(current).isEmpty()) {
        const QVector<Game::NodeId> &kids = game.children(current);
        const Game::NodeId main = kids.first();
        writeMove(game, main, force, tokens);
        force = !game.comment(main).isEmpty() || game.evaluation(main).valid;
        for (int k = 1; k < kids.size(); ++k) {
            tokens->append(QStringLiteral("("));
            writeMove(game, kids.at(k), true, tokens);
            writeContinuation(game, kids.at(k), !game.comment(kids.at(k)).isEmpty(), tokens);
            tokens->append(QStringLiteral(")"));
            force = true;
        }
        current = main;
    }
}

int brokenAt(const GameBuilder &b)
{
    return b.brokenDepth >= 0 && b.stack.size() >= b.brokenDepth ? b.brokenDepth : -1;
}

// Attaches a comment to the current move; "[%eval 0.35]" becomes the evaluation.
void addComment(Game *game, QString text)
{
    static const QRegularExpression evalRx(QStringLiteral("\\[%eval\\s+([^\\]\\s]+)\\s*\\]"));
    const QRegularExpressionMatch m = evalRx.match(text);
    if (m.hasMatch()) {
        const Evaluation e = Evaluation::fromString(m.captured(1));
        if (e.valid)
            game->setEvaluation(game->currentNode(), e);
        text.remove(m.capturedStart(), m.capturedLength());
    }
    text = text.simplified();
    if (text.isEmpty())
        return;
    const Game::NodeId id = game->currentNode();
    const QString old = game->comment(id);
    game->setComment(id, old.isEmpty() ? text : old + QLatin1Char(' ') + text);
}

} // namespace

QString write(const Game &game)
{
    QString out;
    QMap<QString, QString> tags = game.tags();
    tags.insert(QStringLiteral("Result"), resultToString(game.result()));

    for (const char *name : kSevenTagRoster) {
        const QString n = QLatin1String(name);
        const QString v = tags.value(n);
        out += QStringLiteral("[%1 \"%2\"]\n").arg(n, escapeTagValue(v.isEmpty() ? defaultTagValue(n) : v));
        tags.remove(n);
    }
    tags.remove(QStringLiteral("SetUp"));
    tags.remove(QStringLiteral("FEN"));
    const Position &start = game.startPosition();
    if (start.toFen() != QLatin1String(Position::StartFen)) {
        out += QStringLiteral("[SetUp \"1\"]\n");
        out += QStringLiteral("[FEN \"%1\"]\n").arg(start.toFen());
    }
    for (auto it = tags.constBegin(); it != tags.constEnd(); ++it)
        out += QStringLiteral("[%1 \"%2\"]\n").arg(it.key(), escapeTagValue(it.value()));
    out += QLatin1Char('\n');

    // Movetext with variations, comments, NAGs and evaluations
    QStringList tokens;
    if (!game.comment(Game::RootNode).isEmpty())
        tokens << commentToken(game.comment(Game::RootNode), Evaluation());
    writeContinuation(game, Game::RootNode, true, &tokens);
    tokens << resultToString(game.result());

    QString line;
    for (const QString &t : tokens) {
        if (!line.isEmpty() && line.size() + 1 + t.size() > 79) {
            out += line + QLatin1Char('\n');
            line.clear();
        }
        if (!line.isEmpty() && !line.endsWith(QLatin1Char('(')) && t != QLatin1String(")"))
            line += QLatin1Char(' ');
        line += t;
    }
    out += line + QStringLiteral("\n\n");
    return out;
}

QVector<Game> read(const QString &text, QStringList *errors)
{
    QVector<Game> games;
    GameBuilder b;

    auto finish = [&]() {
        if (b.hasContent) {
            b.begin(errors, games.size() + 1);
            b.game.goToStart();
            b.game.goToLineEnd();
            games.push_back(b.game);
        }
        b = GameBuilder();
    };

    const int n = text.size();
    int i = 0;
    bool lineStart = true;
    while (i < n) {
        const QChar c = text.at(i);

        if (c == QLatin1Char('\n')) {
            lineStart = true;
            ++i;
            continue;
        }
        if (c.isSpace()) {
            ++i;
            continue;
        }
        const bool atLineStart = lineStart;
        lineStart = false;

        // Escape line
        if (c == QLatin1Char('%') && atLineStart) {
            while (i < n && text.at(i) != QLatin1Char('\n'))
                ++i;
            continue;
        }
        // Comments
        if (c == QLatin1Char('{')) {
            const int end = text.indexOf(QLatin1Char('}'), i + 1);
            const QString body = text.mid(i + 1, (end < 0 ? n : end) - i - 1);
            i = end < 0 ? n : end + 1;
            if (b.hasContent && brokenAt(b) < 0) {
                b.begin(errors, games.size() + 1);
                addComment(&b.game, body);
            }
            continue;
        }
        if (c == QLatin1Char(';')) {
            const int start = i + 1;
            while (i < n && text.at(i) != QLatin1Char('\n'))
                ++i;
            if (b.hasContent && brokenAt(b) < 0) {
                b.begin(errors, games.size() + 1);
                addComment(&b.game, text.mid(start, i - start));
            }
            continue;
        }
        // Variations: an alternative to the move just played
        if (c == QLatin1Char('(')) {
            ++i;
            if (b.hasContent) {
                b.begin(errors, games.size() + 1);
                b.stack.append(b.game.currentNode());
                if (brokenAt(b) < 0 && b.game.currentNode() != Game::RootNode)
                    b.game.goBack();
            }
            continue;
        }
        if (c == QLatin1Char(')')) {
            ++i;
            if (!b.stack.isEmpty()) {
                b.game.goToNode(b.stack.takeLast());
                if (b.brokenDepth > b.stack.size())
                    b.brokenDepth = -1;
            }
            continue;
        }
        // Tag pair
        if (c == QLatin1Char('[') && b.stack.isEmpty()) {
            if (b.started) // tags after movetext: next game
                finish();
            int j = i + 1;
            while (j < n && text.at(j).isSpace())
                ++j;
            const int nameStart = j;
            while (j < n && (text.at(j).isLetterOrNumber() || text.at(j) == QLatin1Char('_')))
                ++j;
            const QString name = text.mid(nameStart, j - nameStart);
            while (j < n && text.at(j) != QLatin1Char('"') && text.at(j) != QLatin1Char(']'))
                ++j;
            QString value;
            if (j < n && text.at(j) == QLatin1Char('"')) {
                ++j;
                while (j < n && text.at(j) != QLatin1Char('"')) {
                    if (text.at(j) == QLatin1Char('\\') && j + 1 < n)
                        ++j;
                    value += text.at(j);
                    ++j;
                }
                ++j;
            }
            while (j < n && text.at(j) != QLatin1Char(']') && text.at(j) != QLatin1Char('\n'))
                ++j;
            i = j < n && text.at(j) == QLatin1Char(']') ? j + 1 : j;
            if (!name.isEmpty()) {
                b.tags.insert(name, value);
                b.hasContent = true;
            }
            continue;
        }

        // A token up to the next delimiter
        int j = i;
        while (j < n && !text.at(j).isSpace() && !QStringLiteral("{}();[]").contains(text.at(j)))
            ++j;
        if (j == i) { // stray delimiter such as ']' or '}'
            ++i;
            continue;
        }
        QString token = text.mid(i, j - i);
        i = j;

        if (token.startsWith(QLatin1Char('$'))) {
            bool ok = false;
            const int nag = token.mid(1).toInt(&ok);
            if (ok && b.started && brokenAt(b) < 0 && b.game.currentNode() != Game::RootNode) {
                QVector<int> nags = b.game.nags(b.game.currentNode());
                nags.append(nag);
                b.game.setNags(b.game.currentNode(), nags);
            }
            continue;
        }
        if (isResultToken(token)) {
            if (!b.stack.isEmpty())
                continue; // stray result inside a variation
            b.hasContent = true;
            b.begin(errors, games.size() + 1);
            const Result r = resultFromString(token);
            if (r != Result::Unknown || b.game.result() == Result::Unknown)
                b.game.setResult(r);
            finish();
            continue;
        }
        // Move numbers, possibly glued to the move ("12.e4", "12...Nf6")
        if (!isCastlingToken(token)) {
            int k = 0;
            while (k < token.size() && token.at(k).isDigit())
                ++k;
            if (k > 0 && k < token.size() && token.at(k) == QLatin1Char('.')) {
                while (k < token.size() && token.at(k) == QLatin1Char('.'))
                    ++k;
                token = token.mid(k);
            } else if (k == token.size()) {
                continue; // bare number
            }
        }
        if (token.isEmpty())
            continue;

        b.hasContent = true;
        b.begin(errors, games.size() + 1);
        if (b.broken || brokenAt(b) >= 0)
            continue;
        // move suffixes ! ? !! ?? !? ?! become NAGs
        int nag = 0;
        static const char *const suffixes[] = { "!!", "??", "!?", "?!", "!", "?" };
        static const int suffixNags[] = { 3, 4, 5, 6, 1, 2 };
        for (int k = 0; k < 6; ++k) {
            if (token.endsWith(QLatin1String(suffixes[k]))) {
                nag = suffixNags[k];
                token.chop(int(qstrlen(suffixes[k])));
                break;
            }
        }
        const Move m = parseSan(b.game.position(), token);
        if (m.isNull() || !b.game.addMove(m)) {
            if (errors)
                *errors << QStringLiteral("Game %1: illegal or unreadable move \"%2\" after %3 plies")
                               .arg(games.size() + 1)
                               .arg(token)
                               .arg(b.game.plyCount());
            if (b.stack.isEmpty())
                b.broken = true;          // main line: ignore the rest
            else
                b.brokenDepth = b.stack.size(); // skip the rest of this variation
        } else if (nag) {
            b.game.setNags(b.game.currentNode(), { nag });
        }
    }
    finish();
    return games;
}

QString decode(const QByteArray &bytes)
{
    QByteArray data = bytes;
    if (data.startsWith("\xEF\xBB\xBF"))
        data.remove(0, 3);
    const QString utf8 = QString::fromUtf8(data);
    // Invalid UTF-8 shows up as U+FFFD; then the file is most likely Latin-1.
    if (utf8.contains(QChar::ReplacementCharacter) && !data.contains("\xEF\xBF\xBD"))
        return QString::fromLatin1(data);
    return utf8;
}

bool readFile(const QString &fileName, QVector<Game> *games, QString *error)
{
    QFile f(fileName);
    if (!f.open(QIODevice::ReadOnly)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    QStringList problems;
    *games = read(decode(f.readAll()), &problems);
    if (error)
        *error = problems.join(QLatin1Char('\n'));
    return true;
}

namespace {
bool writeText(const QString &fileName, const QString &text, QIODevice::OpenMode mode, QString *error)
{
    QFile f(fileName);
    if (!f.open(mode)) {
        if (error)
            *error = f.errorString();
        return false;
    }
    const QByteArray data = text.toUtf8();
    if (f.write(data) != data.size()) {
        if (error)
            *error = f.errorString();
        return false;
    }
    return true;
}
} // namespace

bool appendToFile(const QString &fileName, const Game &game, QString *error)
{
    return writeText(fileName, write(game), QIODevice::WriteOnly | QIODevice::Append, error);
}

bool writeFile(const QString &fileName, const Game &game, QString *error)
{
    return writeText(fileName, write(game), QIODevice::WriteOnly | QIODevice::Truncate, error);
}

} // namespace mbc::pgn
