// SPDX-License-Identifier: GPL-3.0-or-later
#include "game.h"

#include "notation.h"

#include <QStringList>

#include <algorithm>

namespace mbc {

QString resultToString(Result r)
{
    switch (r) {
    case Result::WhiteWins: return QStringLiteral("1-0");
    case Result::BlackWins: return QStringLiteral("0-1");
    case Result::Draw:      return QStringLiteral("1/2-1/2");
    case Result::Unknown:   break;
    }
    return QStringLiteral("*");
}

Result resultFromString(const QString &s)
{
    if (s == QLatin1String("1-0"))
        return Result::WhiteWins;
    if (s == QLatin1String("0-1"))
        return Result::BlackWins;
    if (s == QLatin1String("1/2-1/2") || s == QStringLiteral("½-½"))
        return Result::Draw;
    return Result::Unknown;
}

// ---------------------------------------------------------------------------

QString Evaluation::toString() const
{
    if (!valid)
        return QString();
    if (mate)
        return QStringLiteral("#%1").arg(value);
    return QStringLiteral("%1%2").arg(value >= 0 ? QStringLiteral("+") : QString()).arg(value / 100.0, 0, 'f', 2);
}

Evaluation Evaluation::fromString(const QString &s)
{
    Evaluation e;
    QString t = s.trimmed();
    if (t.startsWith(QLatin1Char('#'))) {
        bool ok = false;
        const int v = t.mid(1).toInt(&ok);
        if (ok) {
            e.valid = true;
            e.mate = true;
            e.value = v;
        }
        return e;
    }
    bool ok = false;
    const double v = t.toDouble(&ok);
    if (ok) {
        e.valid = true;
        e.value = int(v * 100.0 + (v >= 0 ? 0.5 : -0.5));
    }
    return e;
}

// ---------------------------------------------------------------------------

Game::Game()
{
    reset();
}

Game::Game(const Position &start)
{
    reset(start);
}

void Game::reset(const Position &start)
{
    m_nodes.clear();
    Node root;
    root.position = start;
    m_nodes.push_back(root);
    m_current = RootNode;
    m_line.assign(1, RootNode);
    m_result = Result::Unknown;
    m_tags.clear();
}

bool Game::isValidNode(NodeId id) const
{
    return id >= 0 && id < int(m_nodes.size()) && node(id).alive;
}

void Game::rebuildLine()
{
    m_line.clear();
    for (NodeId id = m_current; id >= 0; id = node(id).parent)
        m_line.push_back(id);
    std::reverse(m_line.begin(), m_line.end());
}

QStringList Game::sanMoves() const
{
    QStringList l;
    for (std::size_t i = 1; i < m_line.size(); ++i)
        l << node(m_line[i]).san;
    return l;
}

QStringList Game::uciMoves() const
{
    QStringList l;
    for (std::size_t i = 1; i < m_line.size(); ++i)
        l << toUci(node(m_line[i]).move);
    return l;
}

bool Game::addMove(Move m)
{
    const Position &pos = position();
    if (!pos.isLegal(m))
        return false;
    for (const NodeId child : node(m_current).children) {
        if (node(child).move == m) {
            m_current = child;
            m_line.push_back(child);
            return true;
        }
    }
    Node n;
    n.parent = m_current;
    n.move = m;
    n.san = toSan(pos, m);
    n.position = pos.after(m);
    n.ply = node(m_current).ply + 1;
    const NodeId id = NodeId(m_nodes.size());
    m_nodes.push_back(n);
    node(m_current).children.append(id);
    m_current = id;
    m_line.push_back(id);
    return true;
}

void Game::unlink(NodeId id)
{
    // mark the subtree as deleted
    std::vector<NodeId> stack{ id };
    while (!stack.empty()) {
        const NodeId n = stack.back();
        stack.pop_back();
        node(n).alive = false;
        for (const NodeId c : node(n).children)
            stack.push_back(c);
    }
    node(node(id).parent).children.removeAll(id);
}

bool Game::takeBack()
{
    if (m_current == RootNode)
        return false;
    const NodeId gone = m_current;
    m_current = node(gone).parent;
    unlink(gone);
    m_line.pop_back();
    m_result = Result::Unknown;
    return true;
}

int Game::repetitionCount() const
{
    const Position &cur = position();
    int count = 0;
    // Only positions since the last capture or pawn move can repeat.
    const int size = int(m_line.size());
    const int lookBack = qMin(size - 1, cur.halfmoveClock());
    for (int i = size - 1 - lookBack; i < size; ++i) {
        const Position &p = node(m_line[std::size_t(i)]).position;
        if (p.hash() == cur.hash() && p.sideToMove() == cur.sideToMove()
            && p.castlingRights() == cur.castlingRights() && p.enPassantSquare() == cur.enPassantSquare())
            ++count;
    }
    return count;
}

GameStatus Game::status() const
{
    const GameStatus s = position().status();
    if (s == GameStatus::Ongoing && repetitionCount() >= 3)
        return GameStatus::ThreefoldRepetition;
    return s;
}

bool Game::isMainLine(NodeId id) const
{
    for (NodeId n = id; n != RootNode; n = node(n).parent) {
        if (node(node(n).parent).children.value(0, -1) != n)
            return false;
    }
    return true;
}

QVector<Game::NodeId> Game::mainLine() const
{
    QVector<NodeId> line;
    for (NodeId n = RootNode; !node(n).children.isEmpty();) {
        n = node(n).children.first();
        line.append(n);
    }
    return line;
}

void Game::goToNode(NodeId id)
{
    if (!isValidNode(id))
        return;
    m_current = id;
    rebuildLine();
}

bool Game::goBack()
{
    if (m_current == RootNode)
        return false;
    m_current = node(m_current).parent;
    m_line.pop_back();
    return true;
}

bool Game::goForward()
{
    if (node(m_current).children.isEmpty())
        return false;
    m_current = node(m_current).children.first();
    m_line.push_back(m_current);
    return true;
}

void Game::goToLineEnd()
{
    while (goForward()) {
    }
}

bool Game::promoteVariation(NodeId id)
{
    if (!isValidNode(id))
        return false;
    // the nearest ancestor-or-self that is not the first continuation
    for (NodeId n = id; n != RootNode; n = node(n).parent) {
        QVector<NodeId> &siblings = node(node(n).parent).children;
        const int index = siblings.indexOf(n);
        if (index > 0) {
            siblings.move(index, 0);
            return true;
        }
    }
    return false;
}

bool Game::deleteFrom(NodeId id)
{
    if (!isValidNode(id) || id == RootNode)
        return false;
    const bool cursorInside = std::find(m_line.begin(), m_line.end(), id) != m_line.end();
    const NodeId parentId = node(id).parent;
    unlink(id);
    if (cursorInside) {
        m_current = parentId;
        rebuildLine();
    }
    return true;
}

} // namespace mbc
