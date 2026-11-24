// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - notation of a game as a move list (one full move per row) with
// variations and comments. Every move is a link:
// a click goes there; the context menu promotes or deletes variations and edits comments.
#pragma once

#include <chesscore/game.h>

#include <QTextBrowser>

class NotationWidget : public QTextBrowser
{
    Q_OBJECT
public:
    explicit NotationWidget(QWidget *parent = nullptr);

    // The game is not owned; call refresh() after every change.
    void setGame(const mbc::Game *game) { m_game = game; }
    void setPieceLetters(const QString &letters) { m_letters = letters; }
    // Whether the context menu may change the moves (promote, delete); comments always can be edited.
    void setEditable(bool editable) { m_editable = editable; }
    void refresh();

    // "!", "?", "+-", ... for a NAG; "$n" for unknown ones.
    static QString nagSymbol(int nag);

signals:
    void nodeClicked(mbc::Game::NodeId id);
    void promoteRequested(mbc::Game::NodeId id);
    void deleteRequested(mbc::Game::NodeId id);
    void commentRequested(mbc::Game::NodeId id);

protected:
    bool event(QEvent *e) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void changeEvent(QEvent *e) override;

private:
    QString moveHtml(mbc::Game::NodeId id, bool bold) const;
    QString commentHtml(mbc::Game::NodeId id) const;
    void writeMove(QString &html, mbc::Game::NodeId id, bool forceNumber) const;
    void writeLine(QString &html, mbc::Game::NodeId first) const;
    mbc::Game::NodeId nodeAt(const QPoint &pos) const;
    void showCurrent();

    const mbc::Game *m_game = nullptr;
    QString m_letters = QStringLiteral("KQRBN");
    bool m_editable = true;
    mutable QString m_variationColor; // set while a variation is written
};
