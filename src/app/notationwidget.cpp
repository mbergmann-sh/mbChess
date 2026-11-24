// SPDX-License-Identifier: GPL-3.0-or-later
#include "notationwidget.h"

#include <chesscore/notation.h>

#include <QContextMenuEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QScrollBar>
#include <QTextBlock>

using mbc::Game;

namespace {

QString colorName(const QColor &c)
{
    return c.name(QColor::HexRgb);
}

QColor blend(const QColor &a, const QColor &b, double t)
{
    return QColor::fromRgbF(a.redF() * (1 - t) + b.redF() * t, a.greenF() * (1 - t) + b.greenF() * t,
                            a.blueF() * (1 - t) + b.blueF() * t);
}

void chopSpaces(QString &s)
{
    while (s.endsWith(QLatin1Char(' ')))
        s.chop(1);
}

QString anchorName(Game::NodeId id)
{
    return QStringLiteral("n%1").arg(id);
}

} // namespace

NotationWidget::NotationWidget(QWidget *parent)
    : QTextBrowser(parent)
{
    setOpenLinks(false);
    setOpenExternalLinks(false);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    connect(this, &QTextBrowser::anchorClicked, this, [this](const QUrl &url) {
        const QString s = url.toString();
        if (s.startsWith(QLatin1Char('n'))) {
            bool ok = false;
            const int id = s.mid(1).toInt(&ok);
            if (ok)
                emit nodeClicked(id);
        }
    });
}

QString NotationWidget::nagSymbol(int nag)
{
    switch (nag) {
    case 1: return QStringLiteral("!");
    case 2: return QStringLiteral("?");
    case 3: return QStringLiteral("!!");
    case 4: return QStringLiteral("??");
    case 5: return QStringLiteral("!?");
    case 6: return QStringLiteral("?!");
    case 7: return QStringLiteral("□");       // only move
    case 10: return QStringLiteral("=");
    case 13: return QStringLiteral("∞");      // unclear
    case 14: return QStringLiteral("⩲");      // White slightly better
    case 15: return QStringLiteral("⩱");
    case 16: return QStringLiteral("±");
    case 17: return QStringLiteral("∓");
    case 18: return QStringLiteral("+−");
    case 19: return QStringLiteral("−+");
    default: break;
    }
    return QStringLiteral("$%1").arg(nag);
}

QString NotationWidget::moveHtml(Game::NodeId id, bool bold) const
{
    QString text = mbc::localizeSan(m_game->nodeSan(id), m_letters).toHtmlEscaped();
    QString suffix;
    for (const int nag : m_game->nags(id)) {
        const QString sym = nagSymbol(nag);
        // move assessments stick to the move, position assessments follow with a space
        if (nag <= 6)
            text += sym;
        else
            suffix += QLatin1Char(' ') + sym.toHtmlEscaped();
    }
    QString style;
    if (id == m_game->currentNode())
        style = QStringLiteral(" style=\"background-color:%1; color:%2;\"")
                    .arg(colorName(palette().color(QPalette::Highlight)),
                         colorName(palette().color(QPalette::HighlightedText)));
    else if (!m_variationColor.isEmpty())
        style = QStringLiteral(" style=\"color:%1;\"").arg(m_variationColor);
    return QStringLiteral("<a name=\"%1\" href=\"%1\"%2>%3</a>")
               .arg(anchorName(id), style, bold ? QStringLiteral("<b>") + text + QStringLiteral("</b>") : text)
        + suffix;
}

QString NotationWidget::commentHtml(Game::NodeId id) const
{
    const QString comment = m_game->comment(id).trimmed();
    if (comment.isEmpty())
        return QString();
    return QStringLiteral("<span style=\"color:%1;\"><i>%2</i></span>")
        .arg(colorName(blend(palette().color(QPalette::Text), QColor(0, 128, 0), 0.6)), comment.toHtmlEscaped());
}

void NotationWidget::writeMove(QString &html, Game::NodeId id, bool forceNumber) const
{
    const mbc::Position &before = m_game->nodePosition(m_game->parent(id));
    const int number = before.fullmoveNumber();
    if (before.sideToMove() == mbc::White)
        html += QStringLiteral("%1.&nbsp;").arg(number);
    else if (forceNumber)
        html += QStringLiteral("%1...&nbsp;").arg(number);
    html += moveHtml(id, false) + QLatin1Char(' ');
    const QString comment = commentHtml(id);
    if (!comment.isEmpty())
        html += comment + QLatin1Char(' ');
}

// A variation inside a cell of the move list: moves run on as text,
// nested variations in brackets.
void NotationWidget::writeLine(QString &html, Game::NodeId first) const
{
    Game::NodeId n = first;
    bool forceNumber = true;
    for (;;) {
        writeMove(html, n, forceNumber);
        forceNumber = !m_game->comment(n).trimmed().isEmpty();

        const QVector<Game::NodeId> &siblings = m_game->children(m_game->parent(n));
        if (siblings.size() > 1 && siblings.first() == n) {
            for (int i = 1; i < siblings.size(); ++i) {
                html += QLatin1Char('(');
                writeLine(html, siblings.at(i));
                chopSpaces(html);
                html += QStringLiteral(") ");
            }
            forceNumber = true;
        }
        const QVector<Game::NodeId> &next = m_game->children(n);
        if (next.isEmpty())
            break;
        n = next.first();
    }
}

void NotationWidget::refresh()
{
    if (!m_game) {
        clear();
        return;
    }
    const QPalette pal = palette();
    const QColor text = pal.color(QPalette::Text);
    const QColor base = pal.color(QPalette::Base);
    const QString variation = colorName(blend(text, base, 0.35));
    const QString numberColor = colorName(blend(text, base, 0.45));
    const QString stripe = colorName(blend(base, text, 0.06));
    document()->setDefaultStyleSheet(QStringLiteral("a { text-decoration: none; color: %1; }").arg(colorName(text)));

    // Main line as a list: one row per full move "12. | e4 | e5". A comment or
    // variations after a move get a row of their own across the whole width; the
    // list then goes on with "12. | ... | e5".
    const int numberWidth = fontMetrics().horizontalAdvance(QStringLiteral("000.")) + 8;
    QString html;
    const QString rootComment = m_game->comment(Game::RootNode).trimmed();
    if (!rootComment.isEmpty())
        html += QStringLiteral("<p style=\"margin:0;\"><i>%1</i></p>").arg(rootComment.toHtmlEscaped());
    html += QStringLiteral("<table width=\"100%\" cellspacing=\"0\" cellpadding=\"2\">");

    int row = 0;
    bool rowOpen = false;
    auto openRow = [&](int number) {
        html += QStringLiteral("<tr%1><td width=\"%2\" align=\"right\" style=\"color:%3;\">%4.&nbsp;</td>")
                    .arg(row % 2 ? QStringLiteral(" bgcolor=\"%1\"").arg(stripe) : QString())
                    .arg(numberWidth)
                    .arg(numberColor)
                    .arg(number);
        rowOpen = true;
    };
    auto closeRow = [&]() {
        html += QStringLiteral("</tr>");
        rowOpen = false;
        ++row;
    };
    const QString dots = QStringLiteral("<td width=\"45%\" style=\"color:%1;\">...</td>").arg(numberColor);

    Game::NodeId n = m_game->children(Game::RootNode).isEmpty() ? -1 : m_game->children(Game::RootNode).first();
    while (n >= 0) {
        const mbc::Position &before = m_game->nodePosition(m_game->parent(n));
        const bool white = before.sideToMove() == mbc::White;
        if (white) {
            if (rowOpen) {
                html += QStringLiteral("<td></td>");
                closeRow();
            }
            openRow(before.fullmoveNumber());
        } else if (!rowOpen) {
            openRow(before.fullmoveNumber());
            html += dots;
        }
        html += QStringLiteral("<td width=\"45%\">%1</td>").arg(moveHtml(n, true));
        if (!white)
            closeRow();

        // Comment and alternatives to this move
        QString extra = commentHtml(n);
        const QVector<Game::NodeId> &siblings = m_game->children(m_game->parent(n));
        if (siblings.size() > 1 && siblings.first() == n) {
            for (int i = 1; i < siblings.size(); ++i) {
                QString line;
                m_variationColor = variation;
                writeLine(line, siblings.at(i));
                m_variationColor.clear();
                chopSpaces(line);
                extra += QStringLiteral("<p style=\"margin:0; margin-left:%1px; color:%2;\">(%3)</p>")
                             .arg(fontMetrics().averageCharWidth() * 2)
                             .arg(variation, line);
            }
        }
        if (!extra.isEmpty()) {
            if (rowOpen) {
                html += dots;
                closeRow();
            }
            html += QStringLiteral("<tr><td></td><td colspan=\"2\">%1</td></tr>").arg(extra);
        }
        const QVector<Game::NodeId> &next = m_game->children(n);
        n = next.isEmpty() ? -1 : next.first();
    }
    if (rowOpen) {
        html += QStringLiteral("<td></td>");
        closeRow();
    }
    if (m_game->result() != mbc::Result::Unknown)
        html += QStringLiteral("<tr><td></td><td colspan=\"2\"><b>%1</b></td></tr>")
                    .arg(mbc::resultToString(m_game->result()));
    html += QStringLiteral("</table>");

    const int scroll = verticalScrollBar()->value();
    setHtml(html);
    verticalScrollBar()->setValue(scroll);
    showCurrent();
}

void NotationWidget::showCurrent()
{
    const QString name = anchorName(m_game->currentNode());
    for (QTextBlock b = document()->begin(); b.isValid(); b = b.next()) {
        for (auto it = b.begin(); !it.atEnd(); ++it) {
            const QTextFragment f = it.fragment();
            if (f.isValid() && f.charFormat().anchorHref() == name) {
                QTextCursor c(document());
                c.setPosition(f.position() + f.length());
                setTextCursor(c);
                ensureCursorVisible();
                return;
            }
        }
    }
    if (m_game->currentNode() == Game::RootNode)
        verticalScrollBar()->setValue(0);
}

Game::NodeId NotationWidget::nodeAt(const QPoint &pos) const
{
    const QString a = anchorAt(pos);
    if (!a.startsWith(QLatin1Char('n')))
        return -1;
    bool ok = false;
    const int id = a.mid(1).toInt(&ok);
    return ok && m_game && m_game->isValidNode(id) ? id : -1;
}

bool NotationWidget::event(QEvent *e)
{
    // Leave the arrow keys, Home and End to the window's navigation shortcuts.
    if (e->type() == QEvent::ShortcutOverride) {
        auto *k = static_cast<QKeyEvent *>(e);
        switch (k->key()) {
        case Qt::Key_Left:
        case Qt::Key_Right:
        case Qt::Key_Up:
        case Qt::Key_Down:
        case Qt::Key_Home:
        case Qt::Key_End:
            if (k->modifiers() == Qt::NoModifier) {
                e->ignore();
                return false;
            }
            break;
        default:
            break;
        }
    }
    return QTextBrowser::event(e);
}

void NotationWidget::contextMenuEvent(QContextMenuEvent *e)
{
    const Game::NodeId id = nodeAt(e->pos());
    if (id < 0) {
        QTextBrowser::contextMenuEvent(e);
        return;
    }
    QMenu menu(this);
    QAction *promote = menu.addAction(tr("&Promote variation"));
    promote->setEnabled(m_editable && !m_game->isMainLine(id));
    QAction *remove = menu.addAction(tr("&Delete from here"));
    remove->setEnabled(m_editable);
    menu.addSeparator();
    QAction *comment = menu.addAction(tr("Edit &comment..."));
    QAction *chosen = menu.exec(e->globalPos());
    if (chosen == promote)
        emit promoteRequested(id);
    else if (chosen == remove)
        emit deleteRequested(id);
    else if (chosen == comment)
        emit commentRequested(id);
}

void NotationWidget::changeEvent(QEvent *e)
{
    QTextBrowser::changeEvent(e);
    if (e->type() == QEvent::PaletteChange && m_game)
        refresh(); // colours come from the palette
}
