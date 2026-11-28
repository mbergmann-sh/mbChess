// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - engine analysis: an engine of its own searches the position shown on the
// board without time limit ("go infinite") and reports several lines (MultiPV). The
// search restarts whenever the position changes.
#pragma once

#include "enginemanager.h"

#include <chesscore/game.h>
#include <chesscore/position.h>
#include <engine/uciengine.h>

#include <QObject>
#include <QVector>

class EngineAnalysis : public QObject
{
    Q_OBJECT
public:
    struct Line
    {
        int depth = 0;
        mbc::Evaluation eval;   // from White's point of view
        QStringList pv;         // UCI moves from the analysed position
        qint64 nps = 0;
        qint64 nodes = 0;
    };

    explicit EngineAnalysis(QObject *parent = nullptr);
    ~EngineAnalysis() override;

    // Starts (or restarts, if engine or resources changed) the analysis engine.
    void start(const EngineEntry &engine, int threads, int hashMb);
    void stop();                     // quits the engine
    void pause();                    // the engine stops searching, but keeps running
    bool isRunning() const { return m_running; }

    void setMultiPv(int lines);
    int multiPv() const { return m_multiPv; }
    // Lines the engine really shows: 1 if it has no option "MultiPV" (Fruit, Toga, ...).
    int effectiveLines() const;

    // Searches this position (start position of the game + moves) until the next call.
    void analyse(const mbc::Position &startPosition, const QStringList &uciMoves);

    QString engineName() const { return m_name; }
    bool isStarting() const;
    const mbc::Position &position() const { return m_position; }
    const QVector<Line> &lines() const { return m_lines; }

signals:
    void updated();                  // new lines or a new position
    void errorOccurred(const QString &message);

private:
    void onReady();
    void onInfo(const mbc::engine::SearchInfo &info);
    void applyOptions();
    void search();

    mbc::engine::UciEngine *m_engine = nullptr;
    EngineEntry m_entry;
    QString m_name;
    QString m_setup;                 // engine + resources the running engine was started with
    int m_threads = 1;
    int m_hashMb = 64;
    int m_multiPv = 3;
    bool m_running = false;
    bool m_ready = false;
    mbc::Position m_startPosition;
    QStringList m_moves;
    mbc::Position m_position;        // position analysed
    QVector<Line> m_lines;
};
