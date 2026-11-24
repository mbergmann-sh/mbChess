// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - mate search: an engine (preferably Stockfish) looks for a mate in at most N
// moves in the position on the board. Stockfish gets "go mate N", other engines search
// without limit until they report a mate. With several lines (MultiPV) other first moves
// that mate as well (duals) are shown. The solution can be added to the game as a
// variation.
#pragma once

#include "enginemanager.h"

#include <chesscore/position.h>
#include <engine/uciengine.h>

#include <QDialog>
#include <QElapsedTimer>
#include <QMap>

class QCheckBox;
class QComboBox;
class QLabel;
class QPushButton;
class QSpinBox;
class QTextBrowser;
class QTimer;

class MateSearchDialog : public QDialog
{
    Q_OBJECT
public:
    MateSearchDialog(const mbc::Position &position, const QString &pieceLetters, int threads, int hashMb,
                     bool canInsert, QWidget *parent = nullptr);
    ~MateSearchDialog() override;

    // The solution (UCI moves from the position) after "Insert as variation".
    QStringList solution() const { return m_solution; }
    QString solutionComment() const { return m_solutionComment; }

protected:
    void reject() override;

private:
    enum class End { Found, Timeout, Cancelled, EngineDone, Error };

    void start();
    void stopSearch();
    void applyOptions();
    void onReady();
    void onInfo(const mbc::engine::SearchInfo &info);
    void onTick();
    void finish(End how, const QString &error = QString());
    void quitEngine();
    void setRunning(bool running);
    QString sanLine(const QStringList &pv, int maxPlies) const;
    QString scoreText(const mbc::engine::SearchInfo &info) const;

    mbc::Position m_position;
    QString m_letters;
    int m_threads;
    int m_hashMb;
    bool m_canInsert;

    QComboBox *m_engineCombo;
    QSpinBox *m_moves;
    QSpinBox *m_time;
    QCheckBox *m_duals;
    QPushButton *m_startButton;
    QLabel *m_progress;
    QTextBrowser *m_result;
    QPushButton *m_insert;

    QVector<EngineEntry> m_engines;
    mbc::engine::UciEngine *m_engine = nullptr;
    EngineEntry m_entry;
    bool m_running = false;
    bool m_goMate = false;     // Stockfish: "go mate N"
    int m_target = 3;          // mate in at most this many moves
    int m_lineCount = 1;
    QMap<int, mbc::engine::SearchInfo> m_lines; // by MultiPV index
    int m_depth = 0;
    qint64 m_nodes = 0;
    qint64 m_nps = 0;
    QElapsedTimer m_clock;
    qint64 m_foundAt = -1;     // ms: first mate in <= N
    int m_foundDepth = 0;
    QTimer *m_ticker;
    QStringList m_bestPv;      // what "Insert as variation" takes
    QStringList m_solution;
    QString m_solutionComment;
    QString m_bestComment;
    QStringList m_engineErrors;
};
