// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - a game: start position, a tree of moves (main line and variations),
// comments, engine evaluations, result and PGN tags.
//
// The game has a cursor, the "current node". The ply-based functions (plyCount(),
// move(i), position(), uciMoves(), ...) all refer to the line from the start
// position to the current node; that is what the board shows and the engine gets.
#pragma once

#include "position.h"

#include <QMap>
#include <QString>
#include <QVector>

#include <vector>

namespace mbc {

enum class Result { Unknown, WhiteWins, BlackWins, Draw };

QString resultToString(Result r);           // "1-0", "0-1", "1/2-1/2", "*"
Result resultFromString(const QString &s);  // Unknown for anything else

// An engine evaluation, always from White's point of view.
struct Evaluation
{
    bool valid = false;
    bool mate = false;   // value = moves to mate (negative: Black mates)
    int value = 0;       // centipawns, or moves to mate
    int depth = 0;

    QString toString() const; // "+0.35", "-1.20", "#3", "#-2" (as in PGN "[%eval ...]")
    static Evaluation fromString(const QString &s);
};

class Game
{
public:
    using NodeId = int;
    static constexpr NodeId RootNode = 0;

    Game();
    explicit Game(const Position &start);

    void reset(const Position &start = Position::startPosition());

    // --- The current line (start position up to the current node) ---------------
    const Position &startPosition() const { return m_nodes.front().position; }
    const Position &position() const { return node(m_current).position; }
    // Position before ply i of the current line (0 = start); i == plyCount() is position().
    const Position &positionAt(int ply) const { return node(m_line.at(std::size_t(ply))).position; }

    int plyCount() const { return int(m_line.size()) - 1; }
    Move move(int ply) const { return node(m_line.at(std::size_t(ply) + 1)).move; }
    Move lastMove() const { return m_current == RootNode ? Move::null() : node(m_current).move; }
    QString san(int ply) const { return node(m_line.at(std::size_t(ply) + 1)).san; }
    QStringList sanMoves() const;
    QStringList uciMoves() const;

    // Plays a legal move from the current node; returns false (and changes nothing) for
    // an illegal one. If the move already exists there, the cursor just follows it;
    // otherwise it becomes a new continuation (the main one if there was none yet,
    // else a new variation).
    bool addMove(Move m);
    // Removes the current node with everything after it and goes back one move.
    bool takeBack();

    // How often the current position has occurred (same pieces, side, castling, en passant).
    int repetitionCount() const;
    // Status of the current position including threefold repetition.
    GameStatus status() const;

    // --- The tree ---------------------------------------------------------------
    NodeId currentNode() const { return m_current; }
    bool isValidNode(NodeId id) const;
    NodeId parent(NodeId id) const { return node(id).parent; }
    const QVector<NodeId> &children(NodeId id) const { return node(id).children; }
    Move nodeMove(NodeId id) const { return node(id).move; }
    QString nodeSan(NodeId id) const { return node(id).san; }
    const Position &nodePosition(NodeId id) const { return node(id).position; }
    int nodePly(NodeId id) const { return node(id).ply; } // 0 = start position
    QString comment(NodeId id) const { return node(id).comment; }  // text after the move
    void setComment(NodeId id, const QString &text) { node(id).comment = text; }
    const QVector<int> &nags(NodeId id) const { return node(id).nags; } // $1 = !, $2 = ?, ...
    void setNags(NodeId id, const QVector<int> &nags) { node(id).nags = nags; }
    Evaluation evaluation(NodeId id) const { return node(id).eval; }
    void setEvaluation(NodeId id, const Evaluation &e) { node(id).eval = e; }

    // true if the node lies on the main line (first continuation from the start)
    bool isMainLine(NodeId id) const;
    // Main line from the start: node ids, without the root.
    QVector<NodeId> mainLine() const;
    // The current line: node ids from the root (included) to the current node.
    const std::vector<NodeId> &currentLine() const { return m_line; }

    // --- Navigation ---------------------------------------------------------------
    void goToNode(NodeId id);
    bool goBack();
    bool goForward();       // into the first continuation
    void goToStart() { goToNode(RootNode); }
    void goToLineEnd();     // follow the first continuations from the current node
    bool isAtLineEnd() const { return node(m_current).children.isEmpty(); }

    // --- Editing variations ---------------------------------------------------------
    // Makes the variation containing id the main continuation at its branching point.
    bool promoteVariation(NodeId id);
    // Deletes the node with everything after it. The cursor moves to its parent if
    // it was inside the deleted part.
    bool deleteFrom(NodeId id);

    Result result() const { return m_result; }
    void setResult(Result r) { m_result = r; }

    // PGN tag pairs ("Event", "White", ...). FEN/SetUp are written automatically.
    QMap<QString, QString> &tags() { return m_tags; }
    const QMap<QString, QString> &tags() const { return m_tags; }
    QString tag(const QString &name) const { return m_tags.value(name); }
    void setTag(const QString &name, const QString &value) { m_tags.insert(name, value); }

private:
    struct Node
    {
        NodeId parent = -1;
        Move move = Move::null();
        QString san;
        Position position;  // after the move
        int ply = 0;
        QVector<NodeId> children; // first = main continuation
        QString comment;
        QVector<int> nags;
        Evaluation eval;
        bool alive = true;
    };

    Node &node(NodeId id) { return m_nodes[std::size_t(id)]; }
    const Node &node(NodeId id) const { return m_nodes[std::size_t(id)]; }
    void rebuildLine();
    void unlink(NodeId id);

    std::vector<Node> m_nodes;   // m_nodes[0] = start position; deleted nodes stay as "dead"
    std::vector<NodeId> m_line;  // root .. current
    NodeId m_current = RootNode;
    Result m_result = Result::Unknown;
    QMap<QString, QString> m_tags;
};

} // namespace mbc
