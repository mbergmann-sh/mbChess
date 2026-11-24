// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - controls a chess engine running as a separate process. It speaks UCI or,
// for older engines such as Crafty, the XBoard (CECP) protocol; both look the same to
// the program: options, "go" with a position, search info and the best move in UCI
// notation.
#pragma once

#include <QList>
#include <QPair>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QVector>

#include <chesscore/position.h>

namespace mbc::engine {

enum class Protocol { Uci, XBoard };

struct EngineOption
{
    QString name;
    QString type;         // check, spin, combo, button, string
    QString defaultValue;
    QString min;
    QString max;
    QStringList vars;     // combo values
};

struct SearchInfo
{
    int depth = 0;
    int selDepth = 0;
    int multiPv = 1;
    bool hasScore = false;
    bool isMate = false;
    int score = 0;          // centipawns, or moves to mate if isMate (negative: getting mated)
    bool lowerBound = false;
    bool upperBound = false;
    qint64 nodes = 0;
    qint64 nps = 0;
    qint64 timeMs = 0;
    int hashFull = -1;      // per mille
    QStringList pv;         // UCI moves
};

// Time control for one "go" command. Times in milliseconds; -1 = not set.
struct GoParameters
{
    qint64 whiteTime = -1;
    qint64 blackTime = -1;
    qint64 whiteIncrement = 0;
    qint64 blackIncrement = 0;
    int movesToGo = 0;
    qint64 moveTime = -1;
    int depth = 0;
    int mate = 0;           // UCI "go mate N": search for a mate in N moves
    bool infinite = false;
    bool ponder = false;    // "go ponder": think on the opponent's time (UCI only)
};

// Parsers (public for unit tests)
// XBoard: the key=value pairs of a "feature" line (values may be quoted).
QList<QPair<QString, QString>> parseFeatureLine(const QString &line);
// XBoard: an option="..." feature, e.g. "Resign -check 1", "Hash -spin 64 1 4096".
bool parseXBoardOption(const QString &text, EngineOption *option);
// XBoard: a thinking line "ply score time nodes pv" (time in centiseconds, pv in SAN
// or coordinates) relative to the position searched. False for other lines.
bool parseXBoardPost(const QString &line, const Position &root, SearchInfo *info);
// XBoard: a move in SAN or coordinate notation as UCI ("Nf3" -> "g1f3"); empty if illegal.
QString xboardMoveToUci(const Position &pos, const QString &text);
bool parseInfoLine(const QString &line, SearchInfo *info);
bool parseOptionLine(const QString &line, EngineOption *option);
QString goCommand(const GoParameters &p);
QString positionCommand(const QString &startFen, const QStringList &uciMoves); // empty FEN = startpos

// Finds an engine executable by base name (e.g. "stockfish"): first in extraDirs,
// then in PATH and common install locations. Returns an empty string if not found.
// (Used by the unit tests; the program itself only looks in its "engines" folder.)
QString findEngineExecutable(const QString &baseName, const QStringList &extraDirs = {});

// An engine found in the program's "engines" folder.
struct EngineInfo
{
    QString name;          // display name, e.g. "Stockfish"
    QString path;          // absolute path of the executable
    QStringList arguments; // command line needed for UCI mode (GNU Chess: --uci)
    Protocol protocol = Protocol::Uci;
};

// The folder "engines" next to the program (inside the bundle on macOS).
// "Engines" is accepted as well on case-sensitive file systems.
QString enginesDirectory();
// All engines in the given folder (default: enginesDirectory()) and its direct
// subfolders, sorted by name. Crafty is recognised as an XBoard engine.
QVector<EngineInfo> findEngines(const QString &directory = QString());
// Display name and UCI arguments for an engine executable.
EngineInfo engineInfo(const QString &path);

class UciEngine : public QObject
{
    Q_OBJECT
public:
    enum class State { NotRunning, Starting, Ready, Thinking };
    Q_ENUM(State)

    explicit UciEngine(QObject *parent = nullptr);
    ~UciEngine() override;

    // Starts the engine; the working directory defaults to the executable's folder.
    void start(const QString &program, const QStringList &arguments = {}, Protocol protocol = Protocol::Uci);
    Protocol protocol() const { return m_protocol; }
    void quit();

    State state() const { return m_state; }
    QString program() const { return m_program; }
    QString name() const { return m_name; }
    QString author() const { return m_author; }
    QList<EngineOption> options() const { return m_options; }
    bool hasOption(const QString &name) const;
    EngineOption option(const QString &name) const;

    // Sent immediately when the engine is ready, otherwise queued until "uciok".
    void setOption(const QString &name, const QString &value = QString());
    void newGame();
    void go(const QString &startFen, const QStringList &uciMoves, const GoParameters &params);
    // Stops the current search. With discardResult the following bestmove is not reported.
    void stop(bool discardResult = true);
    // Pondering: a search started with GoParameters::ponder on the position after the
    // expected move of the opponent. If the opponent plays it, ponderHit() turns it into
    // the normal search; otherwise stop() it (or go() on the real position).
    bool isPondering() const { return m_pondering; }
    void ponderHit();

signals:
    void ready();
    // After "uciok", before the first "isready": options() are known now.
    void optionsKnown();
    void infoReceived(const mbc::engine::SearchInfo &info);
    void bestMove(const QString &move, const QString &ponder);
    void errorOccurred(const QString &message);
    void stateChanged(mbc::engine::UciEngine::State state);
    void lineReceived(const QString &line);
    void lineSent(const QString &line);

private:
    void setState(State s);
    // XBoard protocol
    void xbStart();
    void xbHandleLine(const QString &line);
    void xbFeaturesDone();
    void xbSetOption(const QString &name, const QString &value);
    void xbGo(const QString &startFen, const QStringList &uciMoves, const GoParameters &params);
    void xbStop(bool discardResult);
    void xbSendMove(const QString &uci);

    void send(const QString &line);
    void onReadyRead();
    void handleLine(const QString &line);

    QProcess *m_process = nullptr;
    QByteArray m_buffer;
    State m_state = State::NotRunning;
    QString m_program;
    QString m_name;
    QString m_author;
    QList<EngineOption> m_options;
    QStringList m_pendingCommands;
    bool m_uciOk = false;
    int m_discardBestMoves = 0;   // oldest outstanding searches whose result is dropped
    int m_outstandingSearches = 0;
    bool m_currentDiscarded = false;
    bool m_stopSent = false;
    bool m_pondering = false;

    Protocol m_protocol = Protocol::Uci;
    // XBoard state
    bool m_xbFeaturesDone = false;
    bool m_xbUsermove = false;
    bool m_xbSetboard = true;
    bool m_xbAnalyze = false;
    bool m_xbAnalyzing = false;
    int m_xbPing = 0;
    int m_xbReadyPing = -1;   // pong that makes the engine ready
    int m_xbIgnoreUntilPong = -1; // moves before this pong belong to a cancelled search
    QString m_xbSyncedFen;      // what the engine's board holds: start position ...
    QStringList m_xbSyncedMoves; // ... and moves (UCI)
    bool m_xbSynced = false;
    Position m_xbRoot;          // position of the running search
    QStringList m_xbRootMoves;
    QString m_xbLastPvMove;     // first move of the last analysis line
};

} // namespace mbc::engine
