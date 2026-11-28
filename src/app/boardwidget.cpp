// SPDX-License-Identifier: GPL-3.0-or-later
#include "boardwidget.h"

#include "themes.h"

#include <compat/compat.h>

#include <QApplication>
#include <QDrag>
#include <QDragEnterEvent>
#include <QMimeData>
#include <QFileInfo>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QRadialGradient>
#include <QRandomGenerator>
#include <QSvgRenderer>
#include <QtMath>

using namespace mbc;

namespace {

const char *const kPieceFiles[12] = { "wP", "wN", "wB", "wR", "wQ", "wK",
                                      "bP", "bN", "bB", "bR", "bQ", "bK" };

// Wood colours (maple / walnut squares, mahogany frame)
const QColor kLight(234, 205, 160);
const QColor kDark(176, 112, 62);
const QColor kFrame(98, 34, 24);
const QColor kCoordinates(240, 222, 180);

const QColor kLastMove(214, 214, 70, 115);
const QColor kSelected(90, 170, 70, 130);
const QColor kTargetDot(30, 60, 20, 105);
const QColor kHover(255, 255, 255, 150);

// Fills rect with a base colour plus fine, slightly wavy grain lines.
void paintWood(QPainter &p, const QRectF &rect, const QColor &base, quint32 seed, bool horizontal)
{
    QRandomGenerator rng(seed);
    const int shade = int(rng.bounded(8)) - 4;
    p.fillRect(rect, base.lighter(100 + shade));

    const qreal length = horizontal ? rect.width() : rect.height();
    const qreal across = horizontal ? rect.height() : rect.width();
    const int lines = qMax(6, int(across / 3));
    p.save();
    p.setClipRect(rect, p.hasClipping() ? Qt::IntersectClip : Qt::ReplaceClip);
    for (int i = 0; i < lines; ++i) {
        const qreal offset = across * (i + rng.generateDouble() * 0.8) / lines;
        const qreal amplitude = across * (0.01 + rng.generateDouble() * 0.03);
        const qreal phase = rng.generateDouble() * 6.283;
        const qreal waves = 0.5 + rng.generateDouble() * 1.5;
        QColor c = base.darker(112 + int(rng.bounded(25)));
        c.setAlpha(35 + int(rng.bounded(45)));
        p.setPen(QPen(c, qMax<qreal>(0.6, across / 180.0)));

        QPainterPath path;
        const int steps = 24;
        for (int s = 0; s <= steps; ++s) {
            const qreal t = qreal(s) / steps;
            const qreal along = t * length;
            const qreal off = offset + amplitude * qSin(phase + t * waves * 6.283);
            const QPointF pt = horizontal ? QPointF(rect.left() + along, rect.top() + off)
                                          : QPointF(rect.left() + off, rect.top() + along);
            if (s == 0)
                path.moveTo(pt);
            else
                path.lineTo(pt);
        }
        p.drawPath(path);
    }
    p.restore();
}

// A round wooden disc with bevelled edge and a shadow to the lower right (piece set
// "mbchess-discs"): light grey wood for White, dark wood for Black.
void paintDisc(QPainter &p, int size, bool white, quint32 seed)
{
    const qreal d = size * 0.86;
    const QRectF disc((size - d) / 2 - size * 0.012, (size - d) / 2 - size * 0.02, d, d);

    // shadow: a few soft rings
    p.save();
    p.setPen(Qt::NoPen);
    for (int i = 6; i >= 1; --i) {
        const qreal grow = size * 0.012 * i;
        p.setBrush(QColor(0, 0, 0, 22));
        p.drawEllipse(disc.translated(size * 0.035, size * 0.045).adjusted(-grow, -grow, grow, grow).adjusted(size * 0.03, size * 0.03, -size * 0.03, -size * 0.03));
    }
    p.restore();

    const QColor base = white ? QColor(196, 192, 190) : QColor(58, 50, 46);
    QPainterPath clip;
    clip.addEllipse(disc);
    p.save();
    p.setClipPath(clip);
    paintWood(p, disc, base, 77 + seed * 13, true);
    // extra dark grain for the black discs, which are otherwise too flat
    if (!white) {
        QRandomGenerator rng(seed * 31 + 5);
        for (int i = 0; i < 9; ++i) {
            QColor c = base.lighter(130 + int(rng.bounded(40)));
            c.setAlpha(40 + int(rng.bounded(40)));
            p.setPen(QPen(c, qMax<qreal>(0.6, d / 120.0)));
            const qreal y = disc.top() + d * (i + rng.generateDouble()) / 9;
            QPainterPath path;
            path.moveTo(disc.left(), y);
            path.cubicTo(disc.left() + d * 0.3, y + d * (rng.generateDouble() - 0.5) * 0.08,
                         disc.left() + d * 0.7, y + d * (rng.generateDouble() - 0.5) * 0.08, disc.right(), y);
            p.drawPath(path);
        }
    }
    // dome shading: brighter at the upper left, darker at the lower right
    QRadialGradient shade(disc.center() - QPointF(d * 0.2, d * 0.22), d * 0.75);
    shade.setColorAt(0.0, QColor(255, 255, 255, white ? 50 : 30));
    shade.setColorAt(0.6, QColor(255, 255, 255, 0));
    shade.setColorAt(1.0, QColor(0, 0, 0, 70));
    p.fillRect(disc, shade);
    p.restore();

    // bevel: light rim at the upper left, dark rim at the lower right
    const qreal rim = qMax<qreal>(1.0, d * 0.045);
    QLinearGradient bevel(disc.topLeft(), disc.bottomRight());
    bevel.setColorAt(0.0, QColor(255, 255, 255, white ? 230 : 170));
    bevel.setColorAt(0.5, QColor(160, 160, 160, 90));
    bevel.setColorAt(1.0, QColor(0, 0, 0, 170));
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QBrush(bevel), rim));
    p.drawEllipse(disc.adjusted(rim / 2, rim / 2, -rim / 2, -rim / 2));
    p.setPen(QPen(QColor(0, 0, 0, 120), qMax<qreal>(0.8, d / 90.0)));
    p.drawEllipse(disc);
}

QColor averageColor(const QImage &img)
{
    const QImage one = img.scaled(1, 1, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return one.isNull() ? QColor(128, 128, 128) : QColor(one.pixel(0, 0));
}

} // namespace

BoardWidget::BoardWidget(QWidget *parent)
    : QWidget(parent)
    , m_position(Position::startPosition())
{
    setBoardTheme(themes::defaultBoard());
    setPieceSet(themes::defaultPieces());

    QSizePolicy sp(QSizePolicy::Expanding, QSizePolicy::Expanding);
    sp.setHeightForWidth(true);
    setSizePolicy(sp);
    setMouseTracking(true);
}

BoardWidget::~BoardWidget() = default;

void BoardWidget::setPosition(const Position &pos)
{
    m_position = pos;
    clearSelection();
    update();
}

const char *const BoardWidget::PieceMimeType = "application/x-mbchess-piece";

void BoardWidget::setEditMode(bool on)
{
    m_editMode = on;
    setAcceptDrops(on);
    clearSelection();
}

void BoardWidget::setSquareMode(bool on)
{
    m_squareMode = on;
    clearSelection();
}

void BoardWidget::setMark(int sq, const QColor &color, bool frame)
{
    if (sq < 0 || sq >= 64)
        return;
    if (color.isValid())
        m_marks.insert(sq, { color, frame });
    else
        m_marks.remove(sq);
    update();
}

void BoardWidget::clearMarks()
{
    if (m_marks.isEmpty())
        return;
    m_marks.clear();
    update();
}

void BoardWidget::setLastMove(Move move)
{
    m_lastMove = move;
    update();
}

void BoardWidget::setFlipped(bool flipped)
{
    if (m_flipped == flipped)
        return;
    m_flipped = flipped;
    m_background = QPixmap(); // coordinates change
    update();
}

void BoardWidget::setMovableColors(bool white, bool black)
{
    m_movable[White] = white;
    m_movable[Black] = black;
    clearSelection();
    update();
}

void BoardWidget::setShowCoordinates(bool show)
{
    m_showCoordinates = show;
    updateGeometryCache();
    update();
}

void BoardWidget::setBoardTheme(const QString &name)
{
    const themes::ThemeInfo info = themes::boardInfo(name);
    QImage light, dark;
    if (!info.folder.isEmpty()) {
        light = QImage(info.folder + QStringLiteral("/light.png"));
        dark = QImage(info.folder + QStringLiteral("/dark.png"));
    }
    QColor lightColor = info.color(QStringLiteral("light"));
    QColor darkColor = info.color(QStringLiteral("dark"));
    if (!lightColor.isValid() && !light.isNull())
        lightColor = averageColor(light);
    if (!darkColor.isValid() && !dark.isNull())
        darkColor = averageColor(dark);

    m_themed = !info.folder.isEmpty() && lightColor.isValid() && darkColor.isValid();
    if (!m_themed && name != themes::classicBoard() && name != themes::defaultBoard()) {
        setBoardTheme(themes::defaultBoard()); // unknown theme
        return;
    }
    m_borderLineColor = QColor();
    if (!m_themed) {
        // the drawn wooden board
        m_boardTheme = themes::classicBoard();
        m_lightTexture = m_darkTexture = QImage();
    } else {
        m_boardTheme = name;
        m_lightColor = lightColor;
        m_darkColor = darkColor;
        m_lightTexture = light.isNull() ? QImage() : light.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_darkTexture = dark.isNull() ? QImage() : dark.convertToFormat(QImage::Format_ARGB32_Premultiplied);
        m_frameColor = info.color(QStringLiteral("frame"));
        m_coordinateColor = info.color(QStringLiteral("coordinates"));
        if (!m_coordinateColor.isValid())
            m_coordinateColor = m_frameColor.isValid() ? (m_frameColor.lightness() > 128 ? QColor(40, 40, 40) : QColor(235, 235, 235))
                                                       : lightColor.lighter(115);
        else if (m_frameColor.isValid() && qAbs(m_frameColor.lightness() - m_coordinateColor.lightness()) < 30)
            m_coordinateColor = m_frameColor.lightness() > 128 ? QColor(40, 40, 40) : QColor(235, 235, 235);
        m_squareTextures = info.values.value(QStringLiteral("texture")) == QLatin1String("square");
        m_borderLineColor = info.color(QStringLiteral("border"));
    }
    m_background = QPixmap();
    update();
}

void BoardWidget::setPieceSet(const QString &name)
{
    QString folder = themes::pieceFolder(name);
    QString used = name;
    // all 12 pieces must be there, as SVG or PNG
    auto complete = [](const QString &dir) {
        if (dir.isEmpty())
            return false;
        for (const char *code : kPieceFiles) {
            const QString base = dir + QLatin1Char('/') + QLatin1String(code);
            if (!QFileInfo::exists(base + QStringLiteral(".svg")) && !QFileInfo::exists(base + QStringLiteral(".png")))
                return false;
        }
        return true;
    };
    if (!complete(folder)) {
        used = themes::defaultPieces();
        folder = themes::pieceFolder(used);
    }
    for (int i = 0; i < 12; ++i) {
        delete m_renderers[i];
        m_renderers[i] = nullptr;
        m_pieceImages[i] = QImage();
        const QString base = folder + QLatin1Char('/') + QLatin1String(kPieceFiles[i]);
        if (QFileInfo::exists(base + QStringLiteral(".svg")))
            m_renderers[i] = new QSvgRenderer(base + QStringLiteral(".svg"), this);
        else
            m_pieceImages[i] = QImage(base + QStringLiteral(".png"));
    }
    m_pieceSet = used;
    m_discs = used == themes::defaultPieces();
    m_pieceCache.clear();
    update();
}

void BoardWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateGeometryCache();
}

void BoardWidget::updateGeometryCache()
{
    const qreal side = qMin(width(), height());
    m_border = m_showCoordinates ? qRound(side * 0.045) : qRound(side * 0.015);
    m_squareSize = qFloor((side - 2 * m_border) / 8.0);
    const qreal boardSide = m_squareSize * 8;
    const qreal x = (width() - boardSide) / 2.0;
    const qreal y = (height() - boardSide) / 2.0;
    m_boardRect = QRectF(qRound(x), qRound(y), boardSide, boardSide);
    m_background = QPixmap();
    if (m_pieceCache.size() > 48)
        m_pieceCache.clear(); // pixmaps are kept per size; drop old sizes now and then
}

QRectF BoardWidget::squareRect(int sq) const
{
    int file = fileOf(sq), rank = rankOf(sq);
    if (m_flipped) {
        file = 7 - file;
        rank = 7 - rank;
    }
    return QRectF(m_boardRect.left() + file * m_squareSize,
                  m_boardRect.top() + (7 - rank) * m_squareSize, m_squareSize, m_squareSize);
}

int BoardWidget::squareAt(const QPointF &p) const
{
    if (!m_boardRect.contains(p) || m_squareSize <= 0)
        return NoSquare;
    int file = qBound(0, int((p.x() - m_boardRect.left()) / m_squareSize), 7);
    int rank = 7 - qBound(0, int((p.y() - m_boardRect.top()) / m_squareSize), 7);
    if (m_flipped) {
        file = 7 - file;
        rank = 7 - rank;
    }
    return makeSquare(file, rank);
}

const QPixmap &BoardWidget::background()
{
    const qreal dpr = devicePixelRatioF();
    if (!m_background.isNull())
        return m_background;

    m_background = QPixmap(size() * dpr);
    m_background.setDevicePixelRatio(dpr);
    m_background.fill(Qt::transparent);
    QPainter p(&m_background);
    p.setRenderHint(QPainter::Antialiasing);

    // Frame: mahogany; for a theme its frame colour, or wood in the theme's colour
    const QRectF frame = m_boardRect.adjusted(-m_border, -m_border, m_border, m_border);
    if (m_themed && m_frameColor.isValid()) {
        p.fillRect(frame, m_frameColor);
        p.setPen(QPen(m_frameColor.darker(140), 1.5));
    } else {
        const QColor frameColor = m_themed ? m_darkColor.darker(170) : kFrame;
        paintWood(p, frame, frameColor, 4711, true);
        p.setPen(QPen(frameColor.darker(160), 1.5));
    }
    p.setBrush(Qt::NoBrush);
    p.drawRect(frame.adjusted(0.75, 0.75, -0.75, -0.75));

    if (m_themed) {
        // Colour first, texture on top (textures may be partly transparent). Small or
        // "square" textures fill one square each; large ones run across the board,
        // scaled as on the Amiga, where a square was about 72 pixels.
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        for (int sq = 0; sq < 64; ++sq) {
            const bool light = (fileOf(sq) + rankOf(sq)) % 2 == 1;
            const QRectF r = squareRect(sq);
            p.fillRect(r, light ? m_lightColor : m_darkColor);
            const QImage &tex = light ? m_lightTexture : m_darkTexture;
            if (tex.isNull())
                continue;
            QBrush brush(tex);
            QTransform t;
            if (m_squareTextures || qMax(tex.width(), tex.height()) <= 128) {
                t.translate(r.left(), r.top());
                t.scale(r.width() / tex.width(), r.height() / tex.height());
            } else {
                const qreal scale = m_squareSize / 72.0;
                t.translate(m_boardRect.left(), m_boardRect.top());
                t.scale(scale, scale);
            }
            brush.setTransform(t);
            p.fillRect(r, brush);
        }
    } else {
        // Squares, grain direction alternating like a real inlaid board
        for (int sq = 0; sq < 64; ++sq) {
            const bool light = (fileOf(sq) + rankOf(sq)) % 2 == 1;
            paintWood(p, squareRect(sq), light ? kLight : kDark, quint32(1000 + sq), light);
        }
    }
    if (m_borderLineColor.isValid()) {
        const qreal w = qMax<qreal>(1.5, m_squareSize / 40.0);
        p.setPen(QPen(m_borderLineColor, w));
        p.drawRect(m_boardRect.adjusted(-w / 2, -w / 2, w / 2, w / 2));
    } else {
        p.setPen(QPen(QColor(40, 15, 8, 160), 1.0));
        p.drawRect(m_boardRect);
    }

    // Coordinates
    if (m_showCoordinates) {
        QFont f = font();
        f.setPixelSize(qMax(8, int(m_border * 0.62)));
        f.setBold(true);
        p.setFont(f);
        p.setPen(m_themed ? m_coordinateColor : kCoordinates);
        for (int i = 0; i < 8; ++i) {
            const int file = m_flipped ? 7 - i : i;
            const int rank = m_flipped ? i : 7 - i;
            const QString fileName(QChar('a' + file));
            const QString rankName(QChar('1' + rank));
            const qreal x = m_boardRect.left() + i * m_squareSize;
            const qreal y = m_boardRect.top() + i * m_squareSize;
            p.drawText(QRectF(x, m_boardRect.bottom(), m_squareSize, m_border), Qt::AlignCenter, fileName);
            p.drawText(QRectF(frame.left(), y, m_border, m_squareSize), Qt::AlignCenter, rankName);
        }
    }
    return m_background;
}

QPixmap BoardWidget::piecePixmap(Piece piece, int size)
{
    const qreal dpr = devicePixelRatioF();
    const quint64 key = (quint64(piece) << 32) | quint64(size);
    auto it = m_pieceCache.constFind(key);
    if (it != m_pieceCache.constEnd())
        return it.value();

    QPixmap pm(QSize(size, size) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    if (m_discs && m_renderers[piece]) {
        paintDisc(p, size, colorOf(piece) == White, quint32(piece));
        const qreal s = size * 0.64;
        m_renderers[piece]->render(&p, QRectF((size - s) / 2 - size * 0.012, (size - s) / 2 - size * 0.02, s, s));
    } else if (m_renderers[piece]) {
        m_renderers[piece]->render(&p, QRectF(0, 0, size, size));
    } else if (!m_pieceImages[piece].isNull()) {
        p.setRenderHint(QPainter::SmoothPixmapTransform);
        const QImage img = m_pieceImages[piece].scaled(QSize(size, size) * dpr, Qt::KeepAspectRatio,
                                                       Qt::SmoothTransformation);
        const QSizeF s = QSizeF(img.size()) / dpr;
        p.drawImage(QRectF(QPointF((size - s.width()) / 2, (size - s.height()) / 2), s), img);
    }
    p.end();
    m_pieceCache.insert(key, pm);
    return pm;
}

void BoardWidget::paintEvent(QPaintEvent *)
{
    if (m_squareSize <= 0)
        updateGeometryCache();

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.drawPixmap(0, 0, background());

    // Last move
    if (!m_lastMove.isNull()) {
        p.fillRect(squareRect(m_lastMove.from()), kLastMove);
        p.fillRect(squareRect(m_lastMove.to()), kLastMove);
    }
    // Training marks
    for (auto it = m_marks.cbegin(); it != m_marks.cend(); ++it) {
        const QRectF r = squareRect(it.key());
        if (it->frame) {
            const qreal w = qMax<qreal>(3.0, m_squareSize * 0.08);
            p.setBrush(Qt::NoBrush);
            p.setPen(QPen(it->color, w));
            p.drawRect(r.adjusted(w / 2, w / 2, -w / 2, -w / 2));
        } else {
            p.fillRect(r, it->color);
        }
    }
    // Selected square
    if (m_selected != NoSquare)
        p.fillRect(squareRect(m_selected), kSelected);

    // King in check
    if (m_position.inCheck()) {
        const QRectF r = squareRect(m_position.kingSquare(m_position.sideToMove()));
        QRadialGradient g(r.center(), r.width() * 0.6);
        g.setColorAt(0.0, QColor(255, 20, 20, 230));
        g.setColorAt(0.5, QColor(230, 20, 20, 150));
        g.setColorAt(1.0, QColor(160, 0, 0, 0));
        p.fillRect(r, g);
    }

    // Pieces
    const int pieceSize = int(m_squareSize);
    for (int sq = 0; sq < 64; ++sq) {
        const Piece pc = m_position.pieceAt(sq);
        if (pc == NoPiece || (m_dragging && sq == m_selected) || sq == m_dragSource)
            continue;
        p.drawPixmap(squareRect(sq).topLeft(), piecePixmap(pc, pieceSize));
    }

    // Legal targets of the selected piece
    if (m_selected != NoSquare) {
        p.setPen(Qt::NoPen);
        for (const Move &m : m_selectedMoves) {
            const QRectF r = squareRect(m.to());
            if (m_position.isCapture(m)) {
                p.setBrush(Qt::NoBrush);
                p.setPen(QPen(kTargetDot, m_squareSize * 0.08));
                p.drawEllipse(r.center(), r.width() * 0.44, r.width() * 0.44);
                p.setPen(Qt::NoPen);
            } else {
                p.setBrush(kTargetDot);
                p.drawEllipse(r.center(), r.width() * 0.15, r.width() * 0.15);
            }
        }
    }

    // Edit mode: square under a dragged piece
    if (m_editMode && m_hover != NoSquare) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kHover, qMax<qreal>(2.0, m_squareSize * 0.05)));
        p.drawRect(squareRect(m_hover).adjusted(1.5, 1.5, -1.5, -1.5));
    }

    // Hovered target while dragging
    if (m_dragging && m_hover != NoSquare && isTarget(m_hover)) {
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(kHover, qMax<qreal>(2.0, m_squareSize * 0.05)));
        p.drawRect(squareRect(m_hover).adjusted(1.5, 1.5, -1.5, -1.5));
    }

    // Dragged piece on top
    if (m_dragging && m_selected != NoSquare) {
        const QPixmap pm = piecePixmap(m_position.pieceAt(m_selected), pieceSize);
        p.drawPixmap(m_dragPos - QPointF(pieceSize / 2.0, pieceSize / 2.0), pm);
    }
}

bool BoardWidget::canMoveFrom(int sq) const
{
    const Piece pc = m_position.pieceAt(sq);
    return pc != NoPiece && colorOf(pc) == m_position.sideToMove() && m_movable[m_position.sideToMove()];
}

void BoardWidget::select(int sq)
{
    m_selected = sq;
    m_selectedMoves.clear();
    for (const Move &m : m_position.legalMoves())
        if (m.from() == sq)
            m_selectedMoves.push_back(m);
    update();
}

void BoardWidget::clearSelection()
{
    m_selected = NoSquare;
    m_selectedMoves.clear();
    m_pressed = m_dragging = false;
    unsetCursor();
    update();
}

bool BoardWidget::isTarget(int sq) const
{
    for (const Move &m : m_selectedMoves)
        if (m.to() == sq)
            return true;
    return false;
}

void BoardWidget::mousePressEvent(QMouseEvent *event)
{
    if (m_editMode) {
        editPress(event);
        return;
    }
    if (m_squareMode) {
        const int sq = squareAt(compat::mousePos(event));
        if (event->button() == Qt::LeftButton && sq != NoSquare)
            emit squareClicked(sq);
        return;
    }
    if (event->button() == Qt::RightButton) {
        clearSelection();
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;

    const QPoint pos = compat::mousePos(event);
    const int sq = squareAt(pos);

    // Second click of click-click input
    if (m_selected != NoSquare && sq != NoSquare && isTarget(sq)) {
        tryMove(sq, compat::mouseGlobalPos(event));
        return;
    }
    if (sq != NoSquare && canMoveFrom(sq)) {
        if (sq != m_selected)
            select(sq);
        m_pressed = true;
        m_pressPos = pos;
        m_dragPos = pos;
        return;
    }
    clearSelection();
}

void BoardWidget::mouseMoveEvent(QMouseEvent *event)
{
    if (m_editMode) {
        editMove(event);
        return;
    }
    const QPoint pos = compat::mousePos(event);
    if (m_pressed && !m_dragging
        && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        m_dragging = true;
        setCursor(Qt::ClosedHandCursor);
    }
    if (m_dragging) {
        m_dragPos = pos;
        m_hover = squareAt(pos);
        update();
        return;
    }
    const int sq = squareAt(pos);
    if (canMoveFrom(sq) || (m_selected != NoSquare && isTarget(sq)))
        setCursor(Qt::PointingHandCursor);
    else
        unsetCursor();
}

void BoardWidget::mouseReleaseEvent(QMouseEvent *event)
{
    if (m_editMode) {
        editRelease(event);
        return;
    }
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    if (!m_dragging)
        return; // plain click: keep the selection for click-click input

    m_dragging = false;
    unsetCursor();
    const int sq = squareAt(compat::mousePos(event));
    if (sq != NoSquare && isTarget(sq))
        tryMove(sq, compat::mouseGlobalPos(event));
    else
        clearSelection(); // dropped outside: piece jumps back
}

void BoardWidget::leaveEvent(QEvent *event)
{
    m_hover = NoSquare;
    QWidget::leaveEvent(event);
}

void BoardWidget::tryMove(int to, const QPoint &globalPos)
{
    MoveList candidates;
    for (const Move &m : m_selectedMoves)
        if (m.to() == to)
            candidates.push_back(m);
    if (candidates.empty())
        return;

    Move chosen = candidates.front();
    if (candidates.size() > 1) { // promotion
        const PieceType promo = m_autoQueen ? Queen : askPromotion(m_position.sideToMove(), globalPos);
        if (promo == NoPieceType) {
            clearSelection();
            return;
        }
        for (const Move &m : candidates)
            if (m.promotion() == promo)
                chosen = m;
    }
    clearSelection();
    emit moveMade(chosen);
}

PieceType BoardWidget::askPromotion(Color color, const QPoint &globalPos)
{
    QMenu menu(this);
    const struct { PieceType type; QString name; } choices[] = {
        { Queen, tr("Queen") }, { Rook, tr("Rook") }, { Bishop, tr("Bishop") }, { Knight, tr("Knight") }
    };
    for (const auto &c : choices) {
        QAction *a = menu.addAction(QIcon(piecePixmap(makePiece(color, c.type), 48)), c.name);
        a->setData(int(c.type));
    }
    menu.setDefaultAction(menu.actions().constFirst());
    const QAction *chosen = menu.exec(globalPos);
    return chosen ? PieceType(chosen->data().toInt()) : NoPieceType;
}

// ---------------------------------------------------------------------------
// Edit mode (board editor)

void BoardWidget::editPress(QMouseEvent *event)
{
    const QPoint pos = compat::mousePos(event);
    const int sq = squareAt(pos);
    if (sq == NoSquare)
        return;
    if (event->button() == Qt::RightButton) {
        if (m_position.pieceAt(sq) != NoPiece)
            placePieceAt(sq, NoPiece);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_pressed = true;
    m_pressPos = pos;
    m_selected = sq;
}

void BoardWidget::editMove(QMouseEvent *event)
{
    const QPoint pos = compat::mousePos(event);
    if (m_pressed && m_selected != NoSquare && m_position.pieceAt(m_selected) != NoPiece
        && (pos - m_pressPos).manhattanLength() >= QApplication::startDragDistance()) {
        const int sq = m_selected;
        m_pressed = false;
        m_selected = NoSquare;
        startPieceDrag(sq, event->modifiers() & Qt::ControlModifier);
        return;
    }
    const int sq = squareAt(pos);
    if (sq != NoSquare && (m_position.pieceAt(sq) != NoPiece || m_placePiece != NoPiece))
        setCursor(m_position.pieceAt(sq) != NoPiece ? Qt::OpenHandCursor : Qt::PointingHandCursor);
    else
        unsetCursor();
}

void BoardWidget::editRelease(QMouseEvent *event)
{
    if (event->button() != Qt::LeftButton || !m_pressed)
        return;
    m_pressed = false;
    const int sq = squareAt(compat::mousePos(event));
    m_selected = NoSquare;
    if (sq == NoSquare || m_placePiece == NoPiece)
        return;
    if (typeOf(m_placePiece) == Pawn && (rankOf(sq) == 0 || rankOf(sq) == 7))
        return;
    // a click puts the chosen piece; the same piece again takes it away
    placePieceAt(sq, m_position.pieceAt(sq) == m_placePiece ? NoPiece : m_placePiece);
}

void BoardWidget::startPieceDrag(int sq, bool copy)
{
    const Piece pc = m_position.pieceAt(sq);
    const int size = int(m_squareSize);
    auto *mime = new QMimeData;
    mime->setData(QString::fromLatin1(PieceMimeType),
                  QByteArray::number(int(pc)) + ' ' + QByteArray::number(copy ? NoSquare : sq));
    auto *drag = new QDrag(this);
    drag->setMimeData(mime);
    drag->setPixmap(piecePixmap(pc, size));
    drag->setHotSpot(QPoint(size / 2, size / 2));
    if (!copy)
        m_dragSource = sq;
    update();
    const Qt::DropAction action = drag->exec(copy ? Qt::CopyAction : Qt::MoveAction);
    m_dragSource = NoSquare;
    m_hover = NoSquare;
    // dropped next to the board or on a palette: the piece is taken off
    if (!copy && drag->target() != this && m_position.pieceAt(sq) == pc) {
        Q_UNUSED(action)
        placePieceAt(sq, NoPiece);
    }
    update();
}

int BoardWidget::dropSquare(const QMimeData *mime, const QPointF &pos, Piece *piece) const
{
    if (!mime->hasFormat(QString::fromLatin1(PieceMimeType)))
        return NoSquare;
    const QList<QByteArray> parts = mime->data(QString::fromLatin1(PieceMimeType)).split(' ');
    bool ok = false;
    const int p = parts.value(0).toInt(&ok);
    if (!ok || p < 0 || p >= int(NoPiece))
        return NoSquare;
    const int sq = squareAt(pos);
    if (sq == NoSquare)
        return NoSquare;
    if (typeOf(Piece(p)) == Pawn && (rankOf(sq) == 0 || rankOf(sq) == 7))
        return NoSquare; // no pawns on the first and last rank
    if (piece)
        *piece = Piece(p);
    return sq;
}

void BoardWidget::dragEnterEvent(QDragEnterEvent *event)
{
    if (m_editMode && event->mimeData()->hasFormat(QString::fromLatin1(PieceMimeType)))
        event->acceptProposedAction();
}

void BoardWidget::dragMoveEvent(QDragMoveEvent *event)
{
    const int sq = dropSquare(event->mimeData(), compat::dropPos(event));
    if (sq != m_hover) {
        m_hover = sq;
        update();
    }
    if (sq != NoSquare)
        event->acceptProposedAction();
    else
        event->ignore();
}

void BoardWidget::dragLeaveEvent(QDragLeaveEvent *event)
{
    m_hover = NoSquare;
    update();
    QWidget::dragLeaveEvent(event);
}

void BoardWidget::dropEvent(QDropEvent *event)
{
    m_hover = NoSquare;
    Piece pc = NoPiece;
    const int sq = dropSquare(event->mimeData(), compat::dropPos(event), &pc);
    if (sq == NoSquare) {
        event->ignore();
        update();
        return;
    }
    int source = NoSquare;
    if (event->source() == this) {
        const QList<QByteArray> parts = event->mimeData()->data(QString::fromLatin1(PieceMimeType)).split(' ');
        source = parts.value(1, "-1").toInt();
    }
    event->acceptProposedAction();
    placePieceAt(sq, pc, source);
}

void BoardWidget::placePieceAt(int sq, Piece piece, int sourceSq)
{
    if (sourceSq == sq)
        return;
    Position pos = m_position;
    if (isValidSquare(sourceSq))
        pos.setPiece(sourceSq, NoPiece);
    if (piece != NoPiece && typeOf(piece) == King) {
        // one king per side: a new king replaces the old one
        const int old = pos.kingSquare(colorOf(piece));
        if (old != NoSquare)
            pos.setPiece(old, NoPiece);
    }
    pos.setPiece(sq, piece);
    m_position = pos;
    update();
    emit positionEdited(m_position);
}
