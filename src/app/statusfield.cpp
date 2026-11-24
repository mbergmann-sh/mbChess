// SPDX-License-Identifier: GPL-3.0-or-later
#include "statusfield.h"

#include <QEvent>
#include <QHelpEvent>
#include <QPainter>
#include <QToolTip>

namespace {
constexpr int Padding = 8;  // left (after the divider) and right
constexpr int Gap = 5;      // between caption and text
constexpr int MinText = 60; // the text never gets narrower than this
}

StatusField::StatusField(QWidget *parent)
    : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
}

void StatusField::setCaption(const QString &caption)
{
    m_caption = caption;
    changed();
}

void StatusField::setText(const QString &text)
{
    if (text == m_text)
        return;
    m_text = text;
    changed();
}

void StatusField::setDivider(bool on)
{
    m_divider = on;
    changed();
}

void StatusField::setMaximumTextWidth(int pixels)
{
    m_maxTextWidth = pixels;
    changed();
}

void StatusField::changed()
{
    updateGeometry();
    update();
}

int StatusField::captionWidth() const
{
    if (m_caption.isEmpty())
        return 0;
    QFont bold = font();
    bold.setBold(true);
    return QFontMetrics(bold).horizontalAdvance(m_caption) + 2 + Gap;
}

int StatusField::textWidth() const
{
    const int w = fontMetrics().horizontalAdvance(m_text) + 4; // + rounding slack
    return m_maxTextWidth > 0 ? qMin(w, m_maxTextWidth) : w;
}

QSize StatusField::sizeHint() const
{
    return QSize(2 * Padding + captionWidth() + qMax(textWidth(), MinText), fontMetrics().height() + 4);
}

QSize StatusField::minimumSizeHint() const
{
    return QSize(2 * Padding + captionWidth() + qMin(textWidth(), MinText), fontMetrics().height() + 4);
}

void StatusField::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    if (m_divider) {
        p.setPen(palette().color(QPalette::Mid));
        p.drawLine(0, 3, 0, height() - 4);
    }
    QRect r = rect().adjusted(Padding, 0, -Padding, 0);
    if (!m_caption.isEmpty()) {
        QFont bold = font();
        bold.setBold(true);
        p.setFont(bold);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, m_caption);
        r.setLeft(r.left() + captionWidth());
        p.setFont(font());
    }
    p.setPen(palette().color(QPalette::WindowText));
    p.drawText(r, Qt::AlignLeft | Qt::AlignVCenter, fontMetrics().elidedText(m_text, Qt::ElideRight, r.width()));
}

bool StatusField::event(QEvent *event)
{
    // a shortened text shows in full as tooltip, unless the field has its own tooltip
    if (event->type() == QEvent::ToolTip && toolTip().isEmpty()) {
        const int room = width() - 2 * Padding - captionWidth();
        if (fontMetrics().horizontalAdvance(m_text) > room) {
            QToolTip::showText(static_cast<QHelpEvent *>(event)->globalPos(), m_text, this);
            return true;
        }
    }
    if (event->type() == QEvent::FontChange || event->type() == QEvent::StyleChange)
        changed();
    return QWidget::event(event);
}
