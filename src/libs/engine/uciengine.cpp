// SPDX-License-Identifier: GPL-3.0-or-later
#include "uciengine.h"

#include <algorithm>
#include <utility>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QPointer>
#include <QStandardPaths>
#include <QTimer>

namespace mbc::engine {

namespace {

QStringList splitTokens(const QString &line)
{
    return line.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
}

} // namespace

bool parseInfoLine(const QString &line, SearchInfo *info)
{
    const QStringList t = splitTokens(line);
    if (t.isEmpty() || t.at(0) != QLatin1String("info"))
        return false;
    SearchInfo r;
    bool useful = false;
    for (int i = 1; i < t.size(); ++i) {
        const QString &k = t.at(i);
        auto next = [&](qint64 *out) {
            if (i + 1 < t.size()) {
                bool ok = false;
                const qint64 v = t.at(i + 1).toLongLong(&ok);
                if (ok)
                    *out = v;
                ++i;
                return ok;
            }
            return false;
        };
        qint64 v = 0;
        if (k == QLatin1String("string")) {
            break; // free text until end of line
        } else if (k == QLatin1String("depth")) {
            if (next(&v)) { r.depth = int(v); useful = true; }
        } else if (k == QLatin1String("seldepth")) {
            if (next(&v)) r.selDepth = int(v);
        } else if (k == QLatin1String("multipv")) {
            if (next(&v)) r.multiPv = int(v);
        } else if (k == QLatin1String("nodes")) {
            if (next(&v)) r.nodes = v;
        } else if (k == QLatin1String("nps")) {
            if (next(&v)) r.nps = v;
        } else if (k == QLatin1String("time")) {
            if (next(&v)) r.timeMs = v;
        } else if (k == QLatin1String("hashfull")) {
            if (next(&v)) r.hashFull = int(v);
        } else if (k == QLatin1String("score")) {
            if (i + 2 < t.size()) {
                const QString kind = t.at(i + 1);
                bool ok = false;
                const int s = t.at(i + 2).toInt(&ok);
                if (ok && (kind == QLatin1String("cp") || kind == QLatin1String("mate"))) {
                    r.hasScore = true;
                    r.isMate = kind == QLatin1String("mate");
                    r.score = s;
                    useful = true;
                }
                i += 2;
                if (i + 1 < t.size() && t.at(i + 1) == QLatin1String("lowerbound")) {
                    r.lowerBound = true;
                    ++i;
                } else if (i + 1 < t.size() && t.at(i + 1) == QLatin1String("upperbound")) {
                    r.upperBound = true;
                    ++i;
                }
            }
        } else if (k == QLatin1String("pv")) {
            r.pv = t.mid(i + 1);
            useful = useful || !r.pv.isEmpty();
            break;
        }
    }
    if (useful && info)
        *info = r;
    return useful;
}

bool parseOptionLine(const QString &line, EngineOption *option)
{
    const QStringList t = splitTokens(line);
    if (t.size() < 4 || t.at(0) != QLatin1String("option") || t.at(1) != QLatin1String("name"))
        return false;
    static const QStringList keywords = { QStringLiteral("name"), QStringLiteral("type"), QStringLiteral("default"),
                                          QStringLiteral("min"), QStringLiteral("max"), QStringLiteral("var") };
    EngineOption o;
    QString key;
    QStringList value;
    auto flush = [&]() {
        const QString v = value.join(QLatin1Char(' '));
        if (key == QLatin1String("name"))
            o.name = v;
        else if (key == QLatin1String("type"))
            o.type = v;
        else if (key == QLatin1String("default"))
            o.defaultValue = v == QLatin1String("<empty>") ? QString() : v;
        else if (key == QLatin1String("min"))
            o.min = v;
        else if (key == QLatin1String("max"))
            o.max = v;
        else if (key == QLatin1String("var"))
            o.vars << v;
        value.clear();
    };
    for (int i = 1; i < t.size(); ++i) {
        // "name" may only start the name once; later words named like keywords belong to values
        // except for the real keywords after the name.
        if (keywords.contains(t.at(i)) && !(key == QLatin1String("name") && t.at(i) == QLatin1String("name"))) {
            if (!key.isEmpty())
                flush();
            key = t.at(i);
        } else {
            value << t.at(i);
        }
    }
    if (!key.isEmpty())
        flush();
    if (o.name.isEmpty() || o.type.isEmpty())
        return false;
    if (option)
        *option = o;
    return true;
}

QString goCommand(const GoParameters &p)
{
    QString s = QStringLiteral("go");
    if (p.mate > 0)
        return s + QStringLiteral(" mate %1").arg(p.mate);
    if (p.ponder)
        s += QStringLiteral(" ponder");
    if (p.infinite)
        return s + QStringLiteral(" infinite");
    if (p.whiteTime >= 0)
        s += QStringLiteral(" wtime %1").arg(p.whiteTime);
    if (p.blackTime >= 0)
        s += QStringLiteral(" btime %1").arg(p.blackTime);
    if (p.whiteTime >= 0 || p.blackTime >= 0) {
        s += QStringLiteral(" winc %1 binc %2").arg(p.whiteIncrement).arg(p.blackIncrement);
        if (p.movesToGo > 0)
            s += QStringLiteral(" movestogo %1").arg(p.movesToGo);
    }
    if (p.moveTime >= 0)
        s += QStringLiteral(" movetime %1").arg(p.moveTime);
    if (p.depth > 0)
        s += QStringLiteral(" depth %1").arg(p.depth);
    if (s == QLatin1String("go") || s == QLatin1String("go ponder"))
        s += QStringLiteral(" infinite");
    return s;
}

QString positionCommand(const QString &startFen, const QStringList &uciMoves)
{
    QString s = startFen.isEmpty() ? QStringLiteral("position startpos")
                                   : QStringLiteral("position fen ") + startFen;
    if (!uciMoves.isEmpty())
        s += QStringLiteral(" moves ") + uciMoves.join(QLatin1Char(' '));
    return s;
}

QString findEngineExecutable(const QString &baseName, const QStringList &extraDirs)
{
    QStringList names = { baseName };
#ifdef Q_OS_WIN
    names.prepend(baseName + QStringLiteral(".exe"));
#endif
    for (const QString &dir : extraDirs) {
        for (const QString &n : names) {
            const QFileInfo fi(QDir(dir).filePath(n));
            if (fi.isFile() && fi.isExecutable())
                return fi.absoluteFilePath();
        }
    }
    const QString inPath = QStandardPaths::findExecutable(baseName);
    if (!inPath.isEmpty())
        return inPath;
    const QStringList common = {
        QStringLiteral("/usr/games"), QStringLiteral("/usr/local/bin"), QStringLiteral("/opt/homebrew/bin"),
        QStringLiteral("/usr/local/games"), QStringLiteral("/snap/bin")
    };
    return QStandardPaths::findExecutable(baseName, common);
}

QString enginesDirectory()
{
    const QDir appDir(QCoreApplication::applicationDirPath());
    for (const QString &name : { QStringLiteral("engines"), QStringLiteral("Engines") }) {
        if (appDir.exists(name))
            return QDir::cleanPath(appDir.absoluteFilePath(name));
    }
    return QDir::cleanPath(appDir.absoluteFilePath(QStringLiteral("engines")));
}

EngineInfo engineInfo(const QString &path)
{
    static const struct { const char *prefix; const char *name; const char *argument; } known[] = {
        { "stockfish",  "Stockfish",  nullptr },
        { "arasan",     "Arasan",     nullptr },
        { "fruit",      "Fruit",      nullptr },
        { "toga",       "Toga II",    nullptr },
        { "glaurung",   "Glaurung",   nullptr },
        { "gnuchess",   "GNU Chess",  "--uci" },
        { "viridithas", "Viridithas", nullptr },
        { "texel",      "Texel",      nullptr },
        { "patricia",   "Patricia",   nullptr },
        { "tarrasch",   "Tarrasch Toy Engine", nullptr },
        { "crafty",     "Crafty",     "xboard" },
    };
    EngineInfo info;
    info.path = QFileInfo(path).absoluteFilePath();
    const QString base = QFileInfo(path).completeBaseName();
    info.name = base;
    for (const auto &k : known) {
        if (base.startsWith(QLatin1String(k.prefix), Qt::CaseInsensitive)) {
            info.name = QLatin1String(k.name);
            if (k.argument)
                info.arguments << QLatin1String(k.argument);
            if (qstrcmp(k.prefix, "crafty") == 0) {
                // XBoard engine; "log off": no log files in the engine folder
                info.protocol = Protocol::XBoard;
                info.arguments << QStringLiteral("log off");
            }
            break;
        }
    }
    return info;
}

namespace {

bool isEngineFile(const QFileInfo &fi)
{
    if (!fi.isFile() || !fi.isExecutable())
        return false;
#ifdef Q_OS_WIN
    if (fi.suffix().compare(QLatin1String("exe"), Qt::CaseInsensitive) != 0)
        return false;
#else
    // scripts and libraries are no engines
    static const QStringList skip = { QStringLiteral("sh"), QStringLiteral("so"), QStringLiteral("dylib"),
                                      QStringLiteral("py"), QStringLiteral("txt"), QStringLiteral("ini") };
    if (skip.contains(fi.suffix(), Qt::CaseInsensitive))
        return false;
#endif
    return true;
}

} // namespace

QVector<EngineInfo> findEngines(const QString &directory)
{
    const QDir root(directory.isEmpty() ? enginesDirectory() : directory);
    QVector<EngineInfo> result;
    if (!root.exists())
        return result;
    QFileInfoList candidates = root.entryInfoList(QDir::Files | QDir::Executable, QDir::Name);
    for (const QFileInfo &sub : root.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        // macOS: an engine folder may hold an application bundle; only plain programs count
        candidates += QDir(sub.absoluteFilePath()).entryInfoList(QDir::Files | QDir::Executable, QDir::Name);
    }
    for (const QFileInfo &fi : std::as_const(candidates)) {
        if (isEngineFile(fi))
            result.append(engineInfo(fi.absoluteFilePath()));
    }
    std::sort(result.begin(), result.end(), [](const EngineInfo &a, const EngineInfo &b) {
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
    });
    return result;
}

UciEngine::UciEngine(QObject *parent)
    : QObject(parent)
{
}

UciEngine::~UciEngine()
{
    quit();
}

void UciEngine::setState(State s)
{
    if (m_state == s)
        return;
    m_state = s;
    emit stateChanged(s);
}

void UciEngine::start(const QString &program, const QStringList &arguments, Protocol protocol)
{
    quit();
    m_protocol = protocol;
    m_program = program;
    m_name = QFileInfo(program).completeBaseName();
    m_author.clear();
    m_options.clear();
    m_buffer.clear();
    m_uciOk = false;
    m_discardBestMoves = 0;
    m_outstandingSearches = 0;
    m_pondering = false;

    m_process = new QProcess(this);
    m_process->setWorkingDirectory(QFileInfo(program).absolutePath());
    m_process->setProcessChannelMode(QProcess::SeparateChannels);
    connect(m_process, &QProcess::readyReadStandardOutput, this, &UciEngine::onReadyRead);
    connect(m_process, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
        if (e == QProcess::FailedToStart)
            emit errorOccurred(tr("The engine \"%1\" could not be started.").arg(m_program));
        else if (e == QProcess::Crashed && m_state != State::NotRunning)
            emit errorOccurred(tr("The engine \"%1\" crashed.").arg(m_name));
        setState(State::NotRunning);
    });
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int, QProcess::ExitStatus) { setState(State::NotRunning); });

    setState(State::Starting);
    m_process->start(program, arguments);
    if (m_protocol == Protocol::XBoard) {
        xbStart();
        return;
    }
    send(QStringLiteral("uci"));
    // Some engines (GNU Chess) miss a "uci" that arrives while they are still
    // starting up. Ask once more if there is no answer after a moment.
    QPointer<QProcess> process = m_process;
    QTimer::singleShot(1500, this, [this, process] {
        if (process && process == m_process && !m_uciOk && m_state == State::Starting) {
            m_options.clear();
            send(QStringLiteral("uci"));
        }
    });
}

void UciEngine::quit()
{
    if (!m_process)
        return;
    QProcess *p = m_process;
    m_process = nullptr;
    p->disconnect(this);
    if (p->state() != QProcess::NotRunning) {
        p->write("quit\n");
        if (!p->waitForFinished(1500)) {
            p->kill();
            p->waitForFinished(1000);
        }
    }
    p->deleteLater();
    m_pendingCommands.clear();
    m_pondering = false;
    setState(State::NotRunning);
}

bool UciEngine::hasOption(const QString &name) const
{
    for (const EngineOption &o : m_options)
        if (o.name.compare(name, Qt::CaseInsensitive) == 0)
            return true;
    return false;
}

EngineOption UciEngine::option(const QString &name) const
{
    for (const EngineOption &o : m_options)
        if (o.name.compare(name, Qt::CaseInsensitive) == 0)
            return o;
    return {};
}

void UciEngine::send(const QString &line)
{
    if (!m_process)
        return;
    m_process->write(line.toUtf8() + '\n');
    emit lineSent(line);
}

void UciEngine::setOption(const QString &name, const QString &value)
{
    if (m_protocol == Protocol::XBoard) {
        xbSetOption(name, value);
        return;
    }
    const QString cmd = value.isNull() ? QStringLiteral("setoption name %1").arg(name)
                                       : QStringLiteral("setoption name %1 value %2").arg(name, value);
    if (m_uciOk)
        send(cmd);
    else
        m_pendingCommands << cmd;
}

void UciEngine::newGame()
{
    if (m_protocol == Protocol::XBoard) {
        m_xbSynced = false; // the next search sets up its position with "setboard"
        return;
    }
    if (m_uciOk) {
        send(QStringLiteral("ucinewgame"));
        send(QStringLiteral("isready"));
    } else {
        m_pendingCommands << QStringLiteral("ucinewgame");
    }
}

void UciEngine::go(const QString &startFen, const QStringList &uciMoves, const GoParameters &params)
{
    if (!m_process)
        return;
    if (m_state == State::Thinking)
        stop(true);
    if (m_protocol == Protocol::XBoard) {
        GoParameters p = params;
        p.ponder = false; // not offered for XBoard engines
        xbGo(startFen, uciMoves, p);
        return;
    }
    send(positionCommand(startFen, uciMoves));
    send(goCommand(params));
    ++m_outstandingSearches;
    m_currentDiscarded = false;
    m_stopSent = false;
    m_pondering = params.ponder;
    setState(State::Thinking);
}

void UciEngine::ponderHit()
{
    if (!m_pondering || m_state != State::Thinking || m_stopSent)
        return;
    m_pondering = false;
    send(QStringLiteral("ponderhit"));
}

void UciEngine::stop(bool discardResult)
{
    if (m_state != State::Thinking)
        return;
    if (m_protocol == Protocol::XBoard) {
        xbStop(discardResult);
        return;
    }
    if (discardResult && !m_currentDiscarded) {
        ++m_discardBestMoves;
        m_currentDiscarded = true;
    }
    if (!m_stopSent) {
        send(QStringLiteral("stop"));
        m_stopSent = true;
    }
    m_pondering = false;
}

void UciEngine::onReadyRead()
{
    if (!m_process)
        return;
    m_buffer += m_process->readAllStandardOutput();
    int nl;
    while ((nl = m_buffer.indexOf('\n')) >= 0) {
        QByteArray raw = m_buffer.left(nl);
        m_buffer.remove(0, nl + 1);
        if (raw.endsWith('\r'))
            raw.chop(1);
        handleLine(QString::fromUtf8(raw));
    }
}

void UciEngine::handleLine(const QString &line)
{
    emit lineReceived(line);
    if (m_protocol == Protocol::XBoard) {
        xbHandleLine(line);
        return;
    }
    const QString trimmed = line.trimmed();
    if (trimmed.isEmpty())
        return;

    if (trimmed.startsWith(QLatin1String("info "))) {
        SearchInfo info;
        if (m_discardBestMoves == 0 && parseInfoLine(trimmed, &info))
            emit infoReceived(info);
    } else if (trimmed.startsWith(QLatin1String("bestmove"))) {
        const QStringList t = splitTokens(trimmed);
        m_outstandingSearches = qMax(0, m_outstandingSearches - 1);
        if (m_outstandingSearches == 0)
            m_pondering = false;
        setState(m_outstandingSearches > 0 ? State::Thinking : State::Ready);
        if (m_discardBestMoves > 0) {
            --m_discardBestMoves;
            return;
        }
        const QString move = t.value(1);
        const QString ponder = t.size() >= 4 && t.at(2) == QLatin1String("ponder") ? t.at(3) : QString();
        emit bestMove(move, ponder);
    } else if (trimmed.contains(QLatin1String("id name "))) {
        // GNU Chess prints its prompt in front of the first answer
        m_name = trimmed.mid(trimmed.indexOf(QLatin1String("id name ")) + 8).trimmed();
    } else if (trimmed.startsWith(QLatin1String("id author "))) {
        m_author = trimmed.mid(10).trimmed();
    } else if (trimmed.startsWith(QLatin1String("option "))) {
        EngineOption o;
        if (parseOptionLine(trimmed, &o))
            m_options << o;
    } else if (trimmed == QLatin1String("uciok")) {
        m_uciOk = true;
        const QStringList pending = m_pendingCommands;
        m_pendingCommands.clear();
        for (const QString &cmd : pending)
            send(cmd);
        // Options some engines only read once, at their first "isready" (e.g. the
        // book of Fruit and Toga), can be set in a slot connected to this signal.
        emit optionsKnown();
        send(QStringLiteral("isready"));
    } else if (trimmed == QLatin1String("readyok")) {
        if (m_state == State::Starting) {
            setState(State::Ready);
            emit ready();
        }
    }
}

} // namespace mbc::engine
