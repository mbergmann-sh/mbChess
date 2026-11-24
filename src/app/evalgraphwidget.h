// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - graph of the engine evaluations along the current line (White's view).
// A click goes to the move under the mouse.
#pragma once

#include <chesscore/game.h>

#include <QWidget>

class EvalGraphWidget : public QWidget
{
    Q_OBJECT
public:
    explicit EvalGraphWidget(QWidget *parent = nullptr);

    void setGame(const mbc::Game *game) { m_game = game; }
    void refresh();

    QSize sizeHint() const override { return QSize(300, 120); }
    QSize minimumSizeHint() const override { return QSize(120, 60); }

    // Maps an evaluation to -1 .. +1 (White winning = +1).
    static double scale(const mbc::Evaluation &e);

signals:
    void nodeClicked(mbc::Game::NodeId id);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    int indexAt(int x) const;
    double xFor(int index) const;
    QRectF plotRect() const;     // the graph without the axis labels
    int moveNumber(int index) const;  // full move number of m_line[index]

    const mbc::Game *m_game = nullptr;
    QVector<mbc::Game::NodeId> m_line;  // the current line up to its end, without the root
};
