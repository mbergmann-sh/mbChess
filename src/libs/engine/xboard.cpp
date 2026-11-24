// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - the XBoard (CECP, "protover 2") side of UciEngine.
//
// XBoard engines keep their own board. mbChess keeps them in "force" mode and, for
// every search, either sends the moves that are new since the last search or sets
// the whole position up again with "setboard". Moves go out in coordinate notation;
// moves and thinking lines coming back may be SAN (Crafty) and are converted to UCI.
#include "uciengine.h"

#include <chesscore/notation.h>

#include <QFileInfo>
#include <QPointer>
#include <QRegularExpression>
#include <QTimer>

namespace mbc::engine {

namespace {

Position positionFor(const QString &startFen, const QStringList &uciMoves, bool *ok = nullptr)
{
    Position pos = Position::startPosition();
    bool good = startFen.isEmpty() || pos.setFromFen(startFen);
    for (const QString &m : uciMoves) {
        const Move mv = parseUci(pos, m);
        if (mv.isNull()) {
            good = false;
            break;
        }
        pos.makeMove(mv);
    }
    if (ok)
        *ok = good;
    return pos;
}

} // namespace

QList<QPair<QString, QString>> parseFeatureLine(const QString &line)
{
    QList<QPair<QString, QString>> result;
    QString s = line.trimmed();
    if (!s.startsWith(QLatin1String("feature")))
        return result;
    int i = 7;
    const int n = s.size();
    while (i < n) {
        while (i < n && s.at(i).isSpace())
            ++i;
        const int keyStart = i;
        while (i < n && s.at(i) != QLatin1Char('=') && !s.at(i).isSpace())
            ++i;
        const QString key = s.mid(keyStart, i - keyStart);
        if (key.isEmpty())
            break;
        if (i >= n || s.at(i) != QLatin1Char('=')) {
            result.append({ key, QString() });
            continue;
        }
        ++i; // '='
        QString value;
        if (i < n && s.at(i) == QLatin1Char('"')) {
            const int end = s.indexOf(QLatin1Char('"'), i + 1);
            value = s.mid(i + 1, (end < 0 ? n : end) - i - 1);
            i = end < 0 ? n : end + 1;
        } else {
            const int start = i;
            while (i < n && !s.at(i).isSpace())
                ++i;
            value = s.mid(start, i - start);
        }
        result.append({ key, value });
    }
    return result;
}

bool parseXBoardOption(const QString &text, EngineOption *option)
{
    // "Name -type default [min max]"; the name may contain spaces
    static const QRegularExpression re(QStringLiteral("^(.+?)\\s+-(check|spin|slider|combo|string|file|path|button|save|reset)\\b\\s*(.*)$"));
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch())
        return false;
    EngineOption o;
    o.name = m.captured(1).trimmed();
    const QString kind = m.captured(2);
    const QString rest = m.captured(3).trimmed();
    if (kind == QLatin1String("check")) {
        o.type = QStringLiteral("check");
        o.defaultValue = rest.startsWith(QLatin1Char('1')) ? QStringLiteral("true") : QStringLiteral("false");
    } else if (kind == QLatin1String("spin") || kind == QLatin1String("slider")) {
        o.type = QStringLiteral("spin");
        const QStringList v = rest.split(QLatin1Char(' '), Qt::SkipEmptyParts);
        o.defaultValue = v.value(0);
        o.min = v.value(1);
        o.max = v.value(2);
    } else if (kind == QLatin1String("combo")) {
        o.type = QStringLiteral("combo");
        for (QString v : rest.split(QStringLiteral("///"))) {
            v = v.trimmed();
            if (v.startsWith(QLatin1Char('*'))) {
                v.remove(0, 1);
                o.defaultValue = v;
            }
            if (!v.isEmpty())
                o.vars << v;
        }
        if (o.defaultValue.isEmpty())
            o.defaultValue = o.vars.value(0);
    } else if (kind == QLatin1String("string") || kind == QLatin1String("file") || kind == QLatin1String("path")) {
        o.type = QStringLiteral("string");
        o.defaultValue = rest;
    } else {
        o.type = QStringLiteral("button");
    }
    if (option)
        *option = o;
    return true;
}

QString xboardMoveToUci(const Position &pos, const QString &text)
{
    QString t = text.trimmed();
    if (t.isEmpty())
        return QString();
    Move m = parseUci(pos, t);
    if (m.isNull())
        m = parseSan(pos, t);
    return m.isNull() ? QString() : toUci(m);
}

bool parseXBoardPost(const QString &line, const Position &root, SearchInfo *info)
{
    // ply score time(cs) nodes [seldepth nps tbhits] pv
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d+)[.&]?\\s+(-?\\d+)\\s+(\\d+)\\s+(\\d+)\\s*(.*)$"));
    const QRegularExpressionMatch m = re.match(line);
    if (!m.hasMatch())
        return false;
    SearchInfo r;
    r.depth = m.captured(1).toInt();
    const int score = m.captured(2).toInt();
    r.timeMs = m.captured(3).toLongLong() * 10;
    r.nodes = m.captured(4).toLongLong();
    if (r.timeMs > 0)
        r.nps = r.nodes * 1000 / r.timeMs;
    r.hasScore = true;
    const int a = qAbs(score);
    if (a >= 100000) { // XBoard convention: 100000 + moves to mate
        r.isMate = true;
        r.score = (score > 0 ? 1 : -1) * qMax(1, a - 100000);
    } else if (a >= 32000 && a <= 32767) { // Crafty: 32767 - plies to mate
        r.isMate = true;
        r.score = (score > 0 ? 1 : -1) * qMax(1, (32767 - a + 1) / 2);
    } else {
        r.score = score;
    }
    // principal variation: skip move numbers, stop at the first word that is no move
    Position pos = root;
    for (QString tok : m.captured(5).split(QLatin1Char(' '), Qt::SkipEmptyParts)) {
        static const QRegularExpression number(QStringLiteral("^\\d+\\.+$"));
        if (number.match(tok).hasMatch() || tok == QLatin1String("...") || tok.startsWith(QLatin1Char('(')))
            continue;
        static const QRegularExpression glued(QStringLiteral("^\\d+\\.+")); // "1.e4"
        tok.remove(glued);
        const QString uci = xboardMoveToUci(pos, tok);
        if (uci.isEmpty())
            break;
        r.pv << uci;
        pos.makeMove(parseUci(pos, uci));
    }
    if (info)
        *info = r;
    return true;
}

// ---------------------------------------------------------------------------

void UciEngine::xbStart()
{
    m_xbFeaturesDone = false;
    m_xbUsermove = false;
    m_xbSetboard = true;
    m_xbAnalyze = false;
    m_xbAnalyzing = false;
    m_xbPing = 0;
    m_xbReadyPing = -1;
    m_xbIgnoreUntilPong = -1;
    m_xbSynced = false;
    m_xbSyncedMoves.clear();
    m_xbSyncedFen.clear();
    send(QStringLiteral("xboard"));
    send(QStringLiteral("protover 2"));
    // Engines without "feature done=1" get two seconds (protocol rule)
    QPointer<QProcess> process = m_process;
    QTimer::singleShot(2000, this, [this, process] {
        if (process && process == m_process && !m_xbFeaturesDone)
            xbFeaturesDone();
    });
}

void UciEngine::xbFeaturesDone()
{
    if (m_xbFeaturesDone)
        return;
    m_xbFeaturesDone = true;
    m_uciOk = true;
    // Options mbChess sets for every engine
    if (!hasOption(QStringLiteral("Hash")))
        m_options.prepend({ QStringLiteral("Hash"), QStringLiteral("spin"), QStringLiteral("64"), QStringLiteral("1"),
                            QStringLiteral("65536"), {} });
    if (!hasOption(QStringLiteral("Threads")))
        m_options.prepend({ QStringLiteral("Threads"), QStringLiteral("spin"), QStringLiteral("1"), QStringLiteral("1"),
                            QStringLiteral("256"), {} });
    const bool crafty = m_name.startsWith(QLatin1String("Crafty"), Qt::CaseInsensitive)
        || QFileInfo(m_program).completeBaseName().startsWith(QLatin1String("crafty"), Qt::CaseInsensitive);
    if (crafty && !hasOption(QStringLiteral("Skill Level")))
        m_options.append({ QStringLiteral("Skill Level"), QStringLiteral("spin"), QStringLiteral("20"), QStringLiteral("0"),
                           QStringLiteral("20"), {} });
    if (crafty && !hasOption(QStringLiteral("SyzygyPath"))) // "tbpath <folder>" + "egtb"
        m_options.append({ QStringLiteral("SyzygyPath"), QStringLiteral("string"), QString(), {}, {}, {} });
    if (crafty && !hasOption(QStringLiteral("OwnBook"))) // its book.bin, switched with "book on|off"
        m_options.append({ QStringLiteral("OwnBook"), QStringLiteral("check"), QStringLiteral("true"), {}, {}, {} });
    const QStringList pending = m_pendingCommands;
    m_pendingCommands.clear();
    for (const QString &cmd : pending)
        send(cmd);
    emit optionsKnown();
    send(QStringLiteral("post"));   // thinking output
    send(QStringLiteral("easy"));   // no pondering
    send(QStringLiteral("force"));
    m_xbReadyPing = ++m_xbPing;
    send(QStringLiteral("ping %1").arg(m_xbReadyPing));
}

void UciEngine::xbSetOption(const QString &name, const QString &value)
{
    QString cmd;
    if (name.compare(QLatin1String("Hash"), Qt::CaseInsensitive) == 0) {
        cmd = QStringLiteral("memory %1").arg(value.toInt());
    } else if (name.compare(QLatin1String("Threads"), Qt::CaseInsensitive) == 0) {
        // Crafty: one thread is "0" (no parallel search), "1" is rejected
        cmd = QStringLiteral("cores %1").arg(value.toInt() <= 1 ? 0 : value.toInt());
    } else if (name.compare(QLatin1String("Skill Level"), Qt::CaseInsensitive) == 0) {
        cmd = QStringLiteral("skill %1").arg(qBound(1, value.toInt() * 5, 100)); // Crafty: 1..100
    } else if (name.compare(QLatin1String("SyzygyPath"), Qt::CaseInsensitive) == 0) {
        // Crafty (compiled with -DSYZYGY): folder, then switch the tables on
        if (value.trimmed().isEmpty() || value == QLatin1String("<empty>")) {
            cmd = QStringLiteral("egtb off");
        } else {
            const QString c1 = QStringLiteral("tbpath ") + value;
            if (m_uciOk)
                send(c1);
            else
                m_pendingCommands << c1;
            cmd = QStringLiteral("egtb");
        }
    } else if (name.compare(QLatin1String("OwnBook"), Qt::CaseInsensitive) == 0) {
        cmd = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 ? QStringLiteral("book on")
                                                                              : QStringLiteral("book off");
    } else {
        if (m_xbFeaturesDone && !hasOption(name))
            return; // the engine has no such option (e.g. a UCI name)
        const EngineOption o = option(name);
        QString v = value;
        if (o.type == QLatin1String("check"))
            v = value.compare(QLatin1String("true"), Qt::CaseInsensitive) == 0 ? QStringLiteral("1") : QStringLiteral("0");
        cmd = value.isNull() ? QStringLiteral("option %1").arg(name) : QStringLiteral("option %1=%2").arg(name, v);
    }
    if (m_uciOk)
        send(cmd);
    else
        m_pendingCommands << cmd;
}

void UciEngine::xbSendMove(const QString &uci)
{
    send(m_xbUsermove ? QStringLiteral("usermove ") + uci : uci);
}

void UciEngine::xbGo(const QString &startFen, const QStringList &uciMoves, const GoParameters &params)
{
    bool ok = true;
    m_xbRoot = positionFor(startFen, uciMoves, &ok);
    m_xbRootMoves = uciMoves;
    if (!ok) {
        emit errorOccurred(tr("Invalid position for the engine."));
        return;
    }
    send(QStringLiteral("force"));
    const bool continues = m_xbSynced && m_xbSyncedFen == startFen && uciMoves.size() >= m_xbSyncedMoves.size()
        && uciMoves.mid(0, m_xbSyncedMoves.size()) == m_xbSyncedMoves;
    if (continues) {
        for (int i = m_xbSyncedMoves.size(); i < uciMoves.size(); ++i)
            xbSendMove(uciMoves.at(i));
    } else {
        if (m_xbSetboard) {
            send(QStringLiteral("setboard ") + (startFen.isEmpty() ? QString::fromLatin1(Position::StartFen) : startFen));
        } else {
            send(QStringLiteral("new"));
            send(QStringLiteral("force"));
        }
        for (const QString &m : uciMoves)
            xbSendMove(m);
    }
    m_xbSynced = true;
    m_xbSyncedFen = startFen;
    m_xbSyncedMoves = uciMoves;

    ++m_outstandingSearches;
    m_currentDiscarded = false;
    m_stopSent = false;
    setState(State::Thinking);

    if (params.infinite) {
        m_xbAnalyzing = true;
        m_xbLastPvMove.clear();
        send(QStringLiteral("analyze"));
        return;
    }
    m_xbAnalyzing = false;
    const bool white = m_xbRoot.sideToMove() == White;
    if (params.moveTime > 0) {
        send(QStringLiteral("st %1").arg(qMax<qint64>(1, (params.moveTime + 500) / 1000)));
    } else if (params.depth > 0) {
        send(QStringLiteral("sd %1").arg(params.depth));
    } else {
        const qint64 own = white ? params.whiteTime : params.blackTime;
        const qint64 opp = white ? params.blackTime : params.whiteTime;
        const qint64 inc = white ? params.whiteIncrement : params.blackIncrement;
        if (own >= 0) {
            const qint64 sec = qMax<qint64>(1, own / 1000);
            send(QStringLiteral("level %1 %2:%3 %4")
                     .arg(params.movesToGo)
                     .arg(sec / 60)
                     .arg(sec % 60, 2, 10, QLatin1Char('0'))
                     .arg(inc / 1000));
            send(QStringLiteral("time %1").arg(own / 10));
            send(QStringLiteral("otim %1").arg(qMax<qint64>(0, opp) / 10));
        }
    }
    send(QStringLiteral("go"));
}

void UciEngine::xbStop(bool discardResult)
{
    if (m_stopSent)
        return;
    m_stopSent = true;
    if (m_xbAnalyzing) {
        send(QStringLiteral("exit"));
        m_xbAnalyzing = false;
        m_outstandingSearches = qMax(0, m_outstandingSearches - 1);
        setState(State::Ready);
        if (!discardResult && !m_xbLastPvMove.isEmpty())
            emit bestMove(m_xbLastPvMove, QString());
        return;
    }
    if (!discardResult) {
        send(QStringLiteral("?")); // move now
        return;
    }
    // Cancel: back to force mode; a move that still arrives before the pong is dropped
    send(QStringLiteral("force"));
    m_xbIgnoreUntilPong = ++m_xbPing;
    send(QStringLiteral("ping %1").arg(m_xbIgnoreUntilPong));
    m_xbSynced = false; // the engine may or may not have played its move
    m_outstandingSearches = qMax(0, m_outstandingSearches - 1);
    setState(State::Ready);
}

void UciEngine::xbHandleLine(const QString &line)
{
    const QString t = line.trimmed();
    if (t.isEmpty())
        return;

    if (t.startsWith(QLatin1String("feature "))) {
        QStringList answers;
        for (const auto &kv : parseFeatureLine(t)) {
            const QString &k = kv.first;
            const QString &v = kv.second;
            bool accepted = true;
            if (k == QLatin1String("done")) {
                // Some engines (Crafty) say done=1 before their last feature lines
                if (v == QLatin1String("1"))
                    QTimer::singleShot(300, this, [this, p = QPointer<QProcess>(m_process)] {
                        if (p && p == m_process)
                            xbFeaturesDone();
                    });
                continue; // no answer for "done"
            } else if (k == QLatin1String("myname")) {
                m_name = v.trimmed();
            } else if (k == QLatin1String("usermove")) {
                m_xbUsermove = v == QLatin1String("1");
            } else if (k == QLatin1String("setboard")) {
                m_xbSetboard = v == QLatin1String("1");
            } else if (k == QLatin1String("analyze")) {
                m_xbAnalyze = v == QLatin1String("1");
            } else if (k == QLatin1String("san")) {
                // we read SAN and coordinate moves alike
            } else if (k == QLatin1String("option")) {
                EngineOption o;
                if (parseXBoardOption(v, &o) && o.type != QLatin1String("button"))
                    m_options << o;
            } else if (k == QLatin1String("variants") || k == QLatin1String("ping") || k == QLatin1String("time")
                       || k == QLatin1String("draw") || k == QLatin1String("sigint") || k == QLatin1String("sigterm")
                       || k == QLatin1String("reuse") || k == QLatin1String("colors") || k == QLatin1String("name")
                       || k == QLatin1String("playother") || k == QLatin1String("memory") || k == QLatin1String("smp")
                       || k == QLatin1String("nps") || k == QLatin1String("debug") || k == QLatin1String("pause")
                       || k == QLatin1String("ics") || k == QLatin1String("egt") || k == QLatin1String("exclude")
                       || k == QLatin1String("setscore") || k == QLatin1String("highlight")) {
                // known; nothing to keep
            } else {
                accepted = false;
            }
            answers << QStringLiteral("%1 %2").arg(accepted ? QStringLiteral("accepted") : QStringLiteral("rejected"), k);
        }
        for (const QString &a : answers)
            send(a);
        return;
    }

    if (t.startsWith(QLatin1String("pong "))) {
        const int n = t.mid(5).trimmed().toInt();
        if (n >= m_xbIgnoreUntilPong)
            m_xbIgnoreUntilPong = -1;
        if (n == m_xbReadyPing && m_state == State::Starting) {
            m_xbReadyPing = -1;
            setState(State::Ready);
            emit ready();
        }
        return;
    }

    // "move Nf3" (also the old form "1. ... Nf3")
    static const QRegularExpression moveRe(QStringLiteral("^(?:move\\s+(\\S+)|\\d+\\.\\s*\\.\\.\\.\\s*(\\S+))$"));
    const QRegularExpressionMatch mm = moveRe.match(t);
    if (mm.hasMatch()) {
        if (m_xbIgnoreUntilPong >= 0 || m_state != State::Thinking || m_xbAnalyzing)
            return; // result of a cancelled search
        const QString text = mm.captured(1).isEmpty() ? mm.captured(2) : mm.captured(1);
        const QString uci = xboardMoveToUci(m_xbRoot, text);
        m_outstandingSearches = qMax(0, m_outstandingSearches - 1);
        setState(m_outstandingSearches > 0 ? State::Thinking : State::Ready);
        if (uci.isEmpty()) {
            m_xbSynced = false;
            emit errorOccurred(tr("The engine played an illegal move: %1").arg(text));
            return;
        }
        // the engine has made the move on its own board
        m_xbSyncedMoves = m_xbRootMoves;
        m_xbSyncedMoves << uci;
        send(QStringLiteral("force"));
        emit bestMove(uci, QString());
        return;
    }

    if (m_state == State::Thinking && m_xbIgnoreUntilPong < 0) {
        SearchInfo info;
        if (parseXBoardPost(t, m_xbRoot, &info)) {
            if (!info.pv.isEmpty())
                m_xbLastPvMove = info.pv.first();
            emit infoReceived(info);
            return;
        }
    }

    if (t.startsWith(QLatin1String("Illegal move")) || t.startsWith(QLatin1String("Error"))) {
        m_xbSynced = false; // set the position up again next time
        return;
    }
    // "resign", results, "Hint:", "tellics...", "offer draw": not needed
}

} // namespace mbc::engine
