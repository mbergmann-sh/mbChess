// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - one section of the status bar: "Caption: text", with a dividing line on the
// left. A text that does not fit is shortened with "..." (the tooltip shows it in full).
#pragma once

#include <QWidget>

class StatusField : public QWidget
{
public:
    explicit StatusField(QWidget *parent = nullptr);

    void setCaption(const QString &caption);
    QString caption() const { return m_caption; }
    void setText(const QString &text);
    QString text() const { return m_text; }
    void setDivider(bool on);              // line on the left (default on)
    void setMaximumTextWidth(int pixels);  // 0: no limit

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    bool event(QEvent *event) override;

private:
    int captionWidth() const;
    int textWidth() const;
    void changed();

    QString m_caption;
    QString m_text;
    bool m_divider = true;
    int m_maxTextWidth = 0;
};
