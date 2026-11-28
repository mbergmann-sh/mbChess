// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - a row of pieces of one colour in the board's piece set, used beside a board
// in edit mode (board editor, Training > Board and memory). Click: piece to put with a
// click on the board; drag: put it with drag & drop. Pieces dropped here are removed.
#pragma once

#include "boardwidget.h"

#include <compat/compat.h>

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>


class PiecePalette : public QWidget
{
    Q_OBJECT
public:
    PiecePalette(mbc::Color color, BoardWidget *board, QWidget *parent)
        : QWidget(parent), m_color(color), m_board(board)
    {
        setAcceptDrops(true);
        setMouseTracking(true);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

    void setSelected(mbc::Piece p)
    {
        m_selected = (p != mbc::NoPiece && mbc::colorOf(p) == m_color) ? p : mbc::NoPiece;
        update();
    }

    QSize sizeHint() const override { return { 6 * 56, 60 }; }
    QSize minimumSizeHint() const override { return { 6 * 30, 60 }; }

signals:
    void pieceChosen(mbc::Piece piece); // NoPiece: none

protected:
    int cellSize() const { return qMin(height() - 4, width() / 6); }
    QRect cellRect(int i) const
    {
        const int c = cellSize();
        const int x0 = (width() - 6 * c) / 2;
        return QRect(x0 + i * c, (height() - c) / 2, c, c);
    }
    int cellAt(const QPoint &p) const
    {
        for (int i = 0; i < 6; ++i)
            if (cellRect(i).contains(p))
                return i;
        return -1;
    }
    mbc::Piece pieceOf(int i) const
    {
        static const mbc::PieceType order[6] = { mbc::King, mbc::Queen, mbc::Rook, mbc::Bishop, mbc::Knight, mbc::Pawn };
        return mbc::makePiece(m_color, order[i]);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        for (int i = 0; i < 6; ++i) {
            const QRect r = cellRect(i);
            const mbc::Piece pc = pieceOf(i);
            if (pc == m_selected) {
                p.setPen(QPen(palette().highlight().color(), 2));
                p.setBrush(palette().highlight().color().lighter(150));
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6, 6);
            } else if (i == m_hover) {
                p.setPen(Qt::NoPen);
                p.setBrush(palette().midlight());
                p.drawRoundedRect(QRectF(r).adjusted(1, 1, -1, -1), 6, 6);
            }
            const int s = int(r.width() * 0.9);
            p.drawPixmap(r.center() - QPoint(s / 2, s / 2), m_board->piecePixmap(pc, s));
        }
    }

    void mousePressEvent(QMouseEvent *e) override
    {
        if (e->button() == Qt::LeftButton) {
            m_pressCell = cellAt(mbc::compat::mousePos(e));
            m_pressPos = mbc::compat::mousePos(e);
        }
    }
    void mouseMoveEvent(QMouseEvent *e) override
    {
        const QPoint pos = mbc::compat::mousePos(e);
        if (m_pressCell >= 0 && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
            const mbc::Piece pc = pieceOf(m_pressCell);
            m_pressCell = -1;
            const int size = qMax(24, m_board->width() / 9);
            auto *mime = new QMimeData;
            mime->setData(QString::fromLatin1(BoardWidget::PieceMimeType), QByteArray::number(int(pc)));
            auto *drag = new QDrag(this);
            drag->setMimeData(mime);
            drag->setPixmap(m_board->piecePixmap(pc, size));
            drag->setHotSpot(QPoint(size / 2, size / 2));
            drag->exec(Qt::CopyAction);
            return;
        }
        const int c = cellAt(pos);
        if (c != m_hover) {
            m_hover = c;
            update();
        }
        setCursor(c >= 0 ? Qt::OpenHandCursor : Qt::ArrowCursor);
    }
    void mouseReleaseEvent(QMouseEvent *e) override
    {
        if (e->button() != Qt::LeftButton || m_pressCell < 0)
            return;
        const int c = cellAt(mbc::compat::mousePos(e));
        if (c == m_pressCell) {
            const mbc::Piece pc = pieceOf(c);
            setSelected(pc == m_selected ? mbc::NoPiece : pc);
            emit pieceChosen(m_selected);
        }
        m_pressCell = -1;
    }
    void leaveEvent(QEvent *e) override
    {
        m_hover = -1;
        update();
        QWidget::leaveEvent(e);
    }
    // a piece from the board dropped here is taken off the board
    void dragEnterEvent(QDragEnterEvent *e) override
    {
        if (e->source() == m_board && e->mimeData()->hasFormat(QString::fromLatin1(BoardWidget::PieceMimeType)))
            e->acceptProposedAction();
    }
    void dropEvent(QDropEvent *e) override { e->acceptProposedAction(); }

private:
    mbc::Color m_color;
    BoardWidget *m_board;
    mbc::Piece m_selected = mbc::NoPiece;
    int m_hover = -1;
    int m_pressCell = -1;
    QPoint m_pressPos;
};
