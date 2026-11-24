// SPDX-License-Identifier: GPL-3.0-or-later
#include "evalgraphwidget.h"

#include <QEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>

#include <cmath>

using mbc::Game;

EvalGraphWidget::EvalGraphWidget(QWidget *parent)
    : QWidget(parent)
{
    setMouseTracking(false);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

double EvalGraphWidget::scale(const mbc::Evaluation &e)
{
    if (!e.valid)
        return 0.0;
    if (e.mate)
        return e.value > 0 ? 1.0 : (e.value < 0 ? -1.0 : 0.0);
    // Winning chances, as many chess sites draw it: +-1 pawn is clearly visible,
    // beyond +-5 pawns the curve flattens out.
    return 2.0 / (1.0 + std::exp(-0.004 * e.value)) - 1.0;
}

void EvalGraphWidget::refresh()
{
    m_line.clear();
    if (m_game) {
        const std::vector<Game::NodeId> &cur = m_game->currentLine();
        for (std::size_t i = 1; i < cur.size(); ++i)
            m_line.append(cur[i]);
        // continue along the first continuations to the end of the line
        Game::NodeId n = m_game->currentNode();
        while (!m_game->children(n).isEmpty()) {
            n = m_game->children(n).first();
            m_line.append(n);
        }
    }
    update();
}

QRectF EvalGraphWidget::plotRect() const
{
    const QFontMetrics fm = fontMetrics();
    const int left = fm.horizontalAdvance(QStringLiteral("+10")) + 8;
    const int bottom = fm.height() + 4;
    return QRectF(left, 4, qMax(10, width() - left - 8), qMax(10, height() - bottom - 4));
}

double EvalGraphWidget::xFor(int index) const
{
    const QRectF plot = plotRect();
    const int count = qMax(1, int(m_line.size()));
    return plot.left() + plot.width() * (index + 1) / count;
}

int EvalGraphWidget::indexAt(int x) const
{
    if (m_line.isEmpty())
        return -1;
    const QRectF plot = plotRect();
    const int count = int(m_line.size());
    const double step = plot.width() / count;
    const int index = int(std::lround((x - plot.left()) / step)) - 1;
    return qBound(-1, index, count - 1);
}

int EvalGraphWidget::moveNumber(int index) const
{
    const mbc::Position &start = m_game->positionAt(0);
    const int startPly = 2 * (start.fullmoveNumber() - 1) + (start.sideToMove() == mbc::Black ? 1 : 0);
    const int ply = startPly + m_game->nodePly(m_line.at(index)) - 1; // ply of this move, from 0
    return ply / 2 + 1;
}

void EvalGraphWidget::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QPalette &pal = palette();

    bool any = false;
    for (int i = 0; i < m_line.size() && !any; ++i)
        any = m_game->evaluation(m_line.at(i)).valid;
    if (!any) {
        p.fillRect(rect(), pal.color(QPalette::Base));
        p.setPen(QPen(pal.color(QPalette::Mid), 1));
        p.drawRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5));
        p.setPen(pal.color(QPalette::PlaceholderText));
        p.drawText(rect().adjusted(6, 4, -6, -4), Qt::AlignCenter | Qt::TextWordWrap,
                   tr("No evaluations yet. They appear when an engine plays or analyses."));
        return;
    }

    const QRectF plot = plotRect();
    const double mid = plot.center().y();
    const double amp = plot.height() / 2.0;
    const auto yFor = [&](double value) { return mid - value * amp; };
    const QColor text = pal.color(QPalette::WindowText);

    // Background: upper half light (White better), lower half mid grey (Black better).
    p.fillRect(QRectF(plot.left(), plot.top(), plot.width(), mid - plot.top()), QColor(238, 238, 238));
    p.fillRect(QRectF(plot.left(), mid, plot.width(), plot.bottom() - mid), QColor(176, 176, 176));

    // Grid and labels: pawn units (the scale is not linear) and move numbers
    QFont small = font();
    small.setPointSizeF(qMax(6.5, small.pointSizeF() * 0.85));
    p.setFont(small);
    const QFontMetrics fm(small);
    for (int pawns : { -5, -3, -2, -1, 0, 1, 2, 3, 5 }) {
        mbc::Evaluation e;
        e.valid = true;
        e.value = pawns * 100;
        const double y = yFor(scale(e));
        if (pawns != 0) {
            p.setPen(QPen(QColor(0, 0, 0, pawns > 0 ? 28 : 40), 1));
            p.drawLine(QPointF(plot.left(), y), QPointF(plot.right(), y));
        }
        // all labels if there is room, otherwise fewer
        const bool roomy = amp >= 5.0 * fm.height();
        if (roomy || pawns == 0 || qAbs(pawns) == 5 || (qAbs(pawns) == 2 && amp >= 2.5 * fm.height())) {
            p.setPen(text);
            const QString label = pawns > 0 ? QStringLiteral("+%1").arg(pawns) : QString::number(pawns);
            p.drawText(QRectF(0, y - fm.height() / 2.0, plot.left() - 5, fm.height()),
                       Qt::AlignRight | Qt::AlignVCenter, label);
        }
    }
    const double pxPerMove = plot.width() * 2.0 / qMax(1, int(m_line.size()));
    int step = 1;
    for (int s : { 1, 2, 5, 10, 20, 50, 100 }) {
        step = s;
        if (pxPerMove * s >= fm.horizontalAdvance(QStringLiteral("000")) + 10)
            break;
    }
    for (int i = 0; i < m_line.size(); ++i) {
        const int n = moveNumber(i);
        const bool whiteMove = i == 0 ? true : moveNumber(i - 1) != n;
        if (!whiteMove || n % step != 0 || (step > 1 && n == 0))
            continue;
        const double x = xFor(i);
        p.setPen(QPen(QColor(0, 0, 0, 30), 1));
        p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
        p.setPen(text);
        p.drawText(QRectF(x - 20, plot.bottom() + 2, 40, fm.height()), Qt::AlignHCenter | Qt::AlignTop,
                   QString::number(n));
    }
    p.setFont(font());

    any = false;
    QPainterPath curve;
    QPainterPath area;
    for (int i = 0; i < m_line.size(); ++i) {
        const mbc::Evaluation e = m_game->evaluation(m_line.at(i));
        if (!e.valid)
            continue;
        const QPointF pt(xFor(i), yFor(scale(e)));
        if (!any) {
            curve.moveTo(pt);
            area.moveTo(pt.x(), mid);
            any = true;
        } else {
            curve.lineTo(pt);
        }
        area.lineTo(pt);
    }
    area.lineTo(area.currentPosition().x(), mid);
    area.closeSubpath();
    p.save();
    p.setClipRect(plot);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(110, 150, 205, 140));
    p.drawPath(area);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor(25, 80, 165), 1.6));
    p.drawPath(curve);
    p.restore();

    p.setPen(QPen(QColor(90, 90, 90), 1));
    p.drawLine(QPointF(plot.left(), mid), QPointF(plot.right(), mid));
    p.setPen(QPen(pal.color(QPalette::Mid), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(plot.adjusted(-0.5, -0.5, 0.5, 0.5));

    // the current move
    if (m_game) {
        const int index = m_line.indexOf(m_game->currentNode());
        if (index >= 0) {
            const double x = xFor(index);
            p.setPen(QPen(QColor(220, 60, 40), 1.5));
            p.drawLine(QPointF(x, plot.top()), QPointF(x, plot.bottom()));
            const mbc::Evaluation e = m_game->evaluation(m_game->currentNode());
            if (e.valid) {
                const QString label = e.toString();
                const QRectF box = QRectF(fontMetrics().boundingRect(label)).adjusted(-3, -1, 3, 1);
                QRectF placed(QPointF(x + 4, plot.top() + 3), box.size());
                if (placed.right() > plot.right() - 2)
                    placed.moveRight(x - 4);
                p.setPen(Qt::NoPen);
                p.setBrush(pal.color(QPalette::ToolTipBase));
                p.drawRoundedRect(placed, 3, 3);
                p.setPen(pal.color(QPalette::ToolTipText));
                p.drawText(placed, Qt::AlignCenter, label);
            }
        }
    }
}

static int eventX(const QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return int(e->position().x());
#else
    return e->x();
#endif
}

void EvalGraphWidget::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton || !m_game)
        return;
    const int index = indexAt(eventX(e));
    emit nodeClicked(index < 0 ? Game::RootNode : m_line.at(index));
}

void EvalGraphWidget::mouseMoveEvent(QMouseEvent *e)
{
    // dragging with the left button scrubs through the game
    if (!(e->buttons() & Qt::LeftButton) || !m_game)
        return;
    const int index = indexAt(eventX(e));
    const Game::NodeId id = index < 0 ? Game::RootNode : m_line.at(index);
    if (id != m_game->currentNode())
        emit nodeClicked(id);
}

void EvalGraphWidget::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange || e->type() == QEvent::PaletteChange)
        update();
    QWidget::changeEvent(e);
}
