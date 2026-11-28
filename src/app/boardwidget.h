// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - graphical chess board: wooden look or a texture theme, SVG or bitmap
// pieces, move input by
// click-click or drag & drop, highlights for selection, targets, last move and check.
#pragma once

#include <chesscore/position.h>

#include <QHash>
#include <QImage>
#include <QPixmap>
#include <QWidget>

class QMimeData;
class QSvgRenderer;

class BoardWidget : public QWidget
{
    Q_OBJECT
public:
    explicit BoardWidget(QWidget *parent = nullptr);
    ~BoardWidget() override;

    void setPosition(const mbc::Position &pos);
    const mbc::Position &position() const { return m_position; }

    void setLastMove(mbc::Move move);
    void setFlipped(bool flipped);
    bool isFlipped() const { return m_flipped; }

    // Which colours the user may move (e.g. only White when playing against an engine).
    void setMovableColors(bool white, bool black);
    void setShowCoordinates(bool show);
    // Promotions become a queen without asking.
    void setAutoQueen(bool on) { m_autoQueen = on; }

    // Themes by name (see themes.h); unknown names fall back to the defaults.
    void setBoardTheme(const QString &name);
    void setPieceSet(const QString &name);
    QString boardTheme() const { return m_boardTheme; }
    QString pieceSet() const { return m_pieceSet; }

    // Board editor: pieces are moved freely (drag & drop, also between board and palette),
    // a click puts the place piece, a right click removes a piece. No legality checks.
    void setEditMode(bool on);
    bool isEditMode() const { return m_editMode; }
    void setPlacePiece(mbc::Piece piece) { m_placePiece = piece; }
    mbc::Piece placePiece() const { return m_placePiece; }
    // Training: a click only reports its square (squareClicked), nothing is moved.
    void setSquareMode(bool on);
    // Coloured marks on squares (training feedback), drawn under the pieces; an
    // invalid colour removes the mark. frame: only a frame around the square.
    void setMark(int sq, const QColor &color, bool frame = false);
    void clearMarks();
    // MIME type of a dragged piece: "<piece>" or "<piece> <source square>"
    static const char *const PieceMimeType;

    // The piece in the current piece set (for palettes and menus).
    QPixmap piecePixmap(mbc::Piece piece, int size);

    QSize sizeHint() const override { return { 560, 560 }; }
    QSize minimumSizeHint() const override { return { 240, 240 }; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return w; }

signals:
    // A legal move chosen by the user (promotion piece already selected).
    void moveMade(mbc::Move move);
    // Square mode: the user clicked this square.
    void squareClicked(int square);
    // Edit mode: the position was changed by the user.
    void positionEdited(const mbc::Position &position);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dragLeaveEvent(QDragLeaveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    void updateGeometryCache();
    QRectF squareRect(int sq) const;
    int squareAt(const QPointF &p) const;
    bool canMoveFrom(int sq) const;
    void select(int sq);
    void clearSelection();
    bool isTarget(int sq) const;
    void tryMove(int to, const QPoint &globalPos);
    mbc::PieceType askPromotion(mbc::Color color, const QPoint &globalPos);

    const QPixmap &background();
    void editPress(QMouseEvent *event);
    void editMove(QMouseEvent *event);
    void editRelease(QMouseEvent *event);
    void startPieceDrag(int sq, bool copy);
    void placePieceAt(int sq, mbc::Piece piece, int sourceSq = mbc::NoSquare);
    int dropSquare(const QMimeData *mime, const QPointF &pos, mbc::Piece *piece = nullptr) const;

    mbc::Position m_position;
    mbc::Move m_lastMove;
    bool m_flipped = false;
    bool m_movable[2] = { true, true };
    bool m_showCoordinates = true;
    bool m_autoQueen = false;

    int m_selected = mbc::NoSquare;
    mbc::MoveList m_selectedMoves;
    bool m_pressed = false;
    bool m_dragging = false;
    QPoint m_pressPos;
    QPointF m_dragPos;
    int m_hover = mbc::NoSquare;
    bool m_editMode = false;
    bool m_squareMode = false;
    struct Mark { QColor color; bool frame; };
    QHash<int, Mark> m_marks;
    mbc::Piece m_placePiece = mbc::NoPiece;
    int m_dragSource = mbc::NoSquare;   // edit mode: square of the dragged piece

    // geometry
    QRectF m_boardRect;
    qreal m_squareSize = 0;
    qreal m_border = 0;

    QString m_boardTheme;
    QString m_pieceSet;
    bool m_themed = false;   // false: the drawn wooden board
    QColor m_lightColor, m_darkColor;
    QImage m_lightTexture;   // optional, drawn over the colour
    QImage m_darkTexture;
    QColor m_frameColor;     // invalid: wood in the theme's colour
    QColor m_coordinateColor;
    QColor m_borderLineColor; // optional line around the squares
    bool m_squareTextures = false; // each texture fills one square
    QSvgRenderer *m_renderers[12] = {};  // SVG pieces ...
    QImage m_pieceImages[12];            // ... or bitmaps
    bool m_discs = false;                // symbols drawn on wooden discs
    QPixmap m_background;
    QHash<quint64, QPixmap> m_pieceCache;
};
