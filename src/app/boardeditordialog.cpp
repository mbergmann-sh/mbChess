// SPDX-License-Identifier: GPL-3.0-or-later
#include "boardeditordialog.h"
#include "piecepalette.h"

#include "boardwidget.h"

#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QDrag>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QRadioButton>
#include <QSaveFile>
#include <QSettings>
#include <QSpinBox>
#include <QTextStream>
#include <QVBoxLayout>

#include <compat/compat.h>

using namespace mbc;


// ---------------------------------------------------------------------------

namespace {

const int kCastlingFlags[4] = { WhiteKingSide, WhiteQueenSide, BlackKingSide, BlackQueenSide };

Piece pieceFromLetter(QChar c)
{
    static const char letters[] = "PNBRQKpnbrqk";
    for (int i = 0; i < 12; ++i)
        if (c == QLatin1Char(letters[i]))
            return Piece(i);
    return NoPiece;
}

} // namespace

BoardEditorDialog::BoardEditorDialog(const Position &start, const Position &current, const QString &boardTheme,
                                     const QString &pieceSet, QWidget *parent)
    : BoardEditorDialog(start, boardTheme, pieceSet, parent, &current)
{
}

BoardEditorDialog::BoardEditorDialog(const Position &start, const QString &boardTheme,
                                     const QString &pieceSet, QWidget *parent, const Position *current)
    : QDialog(parent)
    , m_hasCurrent(current != nullptr)
{
    if (current)
        m_current = *current;
    setWindowTitle(tr("Set up position"));

    m_board = new BoardWidget(this);
    m_board->setBoardTheme(boardTheme);
    m_board->setPieceSet(pieceSet);
    m_board->setEditMode(true);
    m_board->setToolTip(tr("Drag pieces onto the board, or choose a piece above or below and click "
                           "the squares.\nDrag a piece off the board or right-click it to remove it. "
                           "Ctrl+drag copies a piece."));
    m_blackPalette = new PiecePalette(Black, m_board, this);
    m_whitePalette = new PiecePalette(White, m_board, this);
    for (PiecePalette *pal : { m_blackPalette, m_whitePalette }) {
        pal->setToolTip(tr("Drag a piece onto the board, or click it and then click the squares."));
        connect(pal, &PiecePalette::pieceChosen, this, [this](Piece p) {
            m_blackPalette->setSelected(p);
            m_whitePalette->setSelected(p);
            m_board->setPlacePiece(p);
        });
    }
    connect(m_board, &BoardWidget::positionEdited, this, &BoardEditorDialog::onBoardEdited);

    m_fen = new QLineEdit(this);
    m_fen->setToolTip(tr("The position as FEN. You can also type or paste a FEN here (Enter)."));
    connect(m_fen, &QLineEdit::editingFinished, this, &BoardEditorDialog::onFenEdited);
    auto *fenLabel = new QLabel(tr("&FEN:"), this);
    fenLabel->setBuddy(m_fen);
    auto *fenRow = new QHBoxLayout;
    fenRow->addWidget(fenLabel);
    fenRow->addWidget(m_fen, 1);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);

    auto *left = new QVBoxLayout;
    left->addWidget(m_blackPalette);
    left->addWidget(m_board, 1);
    left->addWidget(m_whitePalette);
    left->addLayout(fenRow);
    left->addWidget(m_status);

    // Right column: side to move, then the buttons, then the further details
    auto *sideLabel = new QLabel(tr("Who is to move?"), this);
    QFont bold = sideLabel->font();
    bold.setBold(true);
    sideLabel->setFont(bold);
    m_white = new QRadioButton(tr("&White"), this);
    m_black = new QRadioButton(tr("Blac&k"), this);
    m_auto = new QRadioButton(tr("&Decided by the program"), this);
    m_auto->setToolTip(tr("The side in check is to move, otherwise White."));
    m_sideGroup = new QButtonGroup(this);
    m_sideGroup->addButton(m_white);
    m_sideGroup->addButton(m_black);
    m_sideGroup->addButton(m_auto);
    connect(m_sideGroup, QOverload<QAbstractButton *>::of(&QButtonGroup::buttonClicked), this,
            &BoardEditorDialog::onSideChanged);

    auto *load = new QPushButton(tr("&Load position..."), this);
    auto *save = new QPushButton(tr("&Save position..."), this);
    auto *discard = new QPushButton(tr("Disca&rd"), this);
    m_apply = new QPushButton(tr("&Apply"), this);
    m_apply->setDefault(true);
    discard->setAutoDefault(false);
    load->setAutoDefault(false);
    save->setAutoDefault(false);
    connect(load, &QPushButton::clicked, this, &BoardEditorDialog::loadPosition);
    connect(save, &QPushButton::clicked, this, &BoardEditorDialog::savePosition);
    connect(discard, &QPushButton::clicked, this, &BoardEditorDialog::reject);
    connect(m_apply, &QPushButton::clicked, this, &BoardEditorDialog::accept);

    auto *more = new QGroupBox(tr("Details"), this);
    auto *moreLayout = new QVBoxLayout(more);
    const QString castlingNames[4] = { tr("White O-O"), tr("White O-O-O"), tr("Black O-O"), tr("Black O-O-O") };
    auto *castlingLabel = new QLabel(tr("Castling allowed:"), more);
    moreLayout->addWidget(castlingLabel);
    for (int i = 0; i < 4; ++i) {
        m_castling[i] = new QCheckBox(castlingNames[i], more);
        m_castling[i]->setToolTip(tr("Possible only with king and rook on their initial squares."));
        connect(m_castling[i], &QCheckBox::toggled, this, &BoardEditorDialog::onCastlingChanged);
        moreLayout->addWidget(m_castling[i]);
    }
    auto *form = new QFormLayout;
    m_enPassant = new QComboBox(more);
    m_enPassant->setToolTip(tr("The square a pawn has just passed with a double step."));
    connect(m_enPassant, QOverload<int>::of(&QComboBox::activated), this, &BoardEditorDialog::onEnPassantChanged);
    form->addRow(tr("&En passant:"), m_enPassant);
    m_moveNumber = new QSpinBox(more);
    m_moveNumber->setRange(1, 999);
    connect(m_moveNumber, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &BoardEditorDialog::onMoveNumberChanged);
    form->addRow(tr("&Move number:"), m_moveNumber);
    moreLayout->addLayout(form);
    auto *clear = new QPushButton(tr("&Clear board"), more);
    auto *initial = new QPushButton(tr("&Initial position"), more);
    clear->setAutoDefault(false);
    initial->setAutoDefault(false);
    connect(clear, &QPushButton::clicked, this, &BoardEditorDialog::clearBoard);
    connect(initial, &QPushButton::clicked, this, &BoardEditorDialog::startPosition);
    moreLayout->addWidget(clear);
    moreLayout->addWidget(initial);
    if (m_hasCurrent) {
        auto *fromGame = new QPushButton(tr("C&urrent position"), more);
        fromGame->setToolTip(tr("The position of the game on the main board."));
        fromGame->setAutoDefault(false);
        connect(fromGame, &QPushButton::clicked, this, &BoardEditorDialog::currentPosition);
        moreLayout->addWidget(fromGame);
    }

    auto *right = new QVBoxLayout;
    right->addWidget(sideLabel);
    right->addWidget(m_white);
    right->addWidget(m_black);
    right->addWidget(m_auto);
    right->addSpacing(16);
    right->addWidget(load);
    right->addWidget(save);
    right->addWidget(discard);
    right->addWidget(m_apply);
    right->addSpacing(16);
    right->addWidget(more);
    right->addStretch(1);

    auto *layout = new QHBoxLayout(this);
    layout->addLayout(left, 1);
    layout->addLayout(right);

    const int side = QSettings().value(QStringLiteral("editor/side"), 0).toInt(); // 0 = from the position
    if (side == 2)
        m_auto->setChecked(true);
    else
        (start.sideToMove() == White ? m_white : m_black)->setChecked(true);
    setPosition(start, true);
    resize(860, 680);
}

Color BoardEditorDialog::automaticSideToMove(const Position &pos)
{
    const int wk = pos.kingSquare(White);
    const int bk = pos.kingSquare(Black);
    if (bk != NoSquare && pos.isSquareAttacked(bk, White))
        return Black;
    if (wk != NoSquare && pos.isSquareAttacked(wk, Black))
        return White;
    return White;
}

bool BoardEditorDialog::parseFen(const QString &text, Position *out, QString *error)
{
    const QStringList f = text.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (f.isEmpty()) {
        *error = tr("The FEN is empty.");
        return false;
    }
    Position p;
    const QStringList ranks = f.at(0).split(QLatin1Char('/'));
    if (ranks.size() != 8) {
        *error = tr("The FEN must describe 8 ranks.");
        return false;
    }
    for (int i = 0; i < 8; ++i) {
        const int rank = 7 - i;
        int file = 0;
        for (const QChar c : ranks.at(i)) {
            if (c.isDigit() && c.digitValue() >= 1 && c.digitValue() <= 8) {
                file += c.digitValue();
                continue;
            }
            const Piece pc = pieceFromLetter(c);
            if (pc == NoPiece || file > 7) {
                *error = tr("Rank %1 of the FEN is not valid.").arg(rank + 1);
                return false;
            }
            p.setPiece(makeSquare(file, rank), pc);
            ++file;
        }
        if (file != 8) {
            *error = tr("Rank %1 of the FEN does not have 8 squares.").arg(rank + 1);
            return false;
        }
    }
    p.setSideToMove(f.value(1) == QLatin1String("b") ? Black : White);
    int rights = 0;
    for (const QChar c : f.value(2))
        rights |= c == QLatin1Char('K') ? WhiteKingSide : c == QLatin1Char('Q') ? WhiteQueenSide
                : c == QLatin1Char('k') ? BlackKingSide : c == QLatin1Char('q') ? BlackQueenSide : 0;
    p.setCastlingRights(rights);
    if (f.size() > 3)
        p.setEnPassantSquare(parseSquare(f.at(3)));
    bool ok1 = false, ok2 = false;
    const int half = f.value(4).toInt(&ok1);
    const int full = f.value(5).toInt(&ok2);
    p.setMoveCounters(ok1 ? half : 0, ok2 ? full : 1);
    *out = p;
    return true;
}

QString BoardEditorDialog::validationError(const Position &pos)
{
    for (const Color c : { White, Black }) {
        const QString side = c == White ? tr("White") : tr("Black");
        const int kings = popCount(pos.pieces(c, King));
        if (kings == 0)
            return tr("%1 has no king.").arg(side);
        if (kings > 1)
            return tr("%1 has more than one king.").arg(side);
        const int pawns = popCount(pos.pieces(c, Pawn));
        if (pawns > 8)
            return tr("%1 has more than 8 pawns.").arg(side);
        if (popCount(pos.occupancy(c)) > 16)
            return tr("%1 has more than 16 pieces.").arg(side);
    }
    const Bitboard backRanks = 0xFF000000000000FFULL;
    if ((pos.pieces(White, Pawn) | pos.pieces(Black, Pawn)) & backRanks)
        return tr("Pawns cannot stand on the first or last rank.");
    const Color other = ~pos.sideToMove();
    if (pos.isSquareAttacked(pos.kingSquare(other), pos.sideToMove()))
        return tr("%1 is in check but not to move.").arg(other == White ? tr("White") : tr("Black"));
    return QString();
}

void BoardEditorDialog::setPosition(const Position &pos, bool takeCastling)
{
    m_position = pos;
    if (takeCastling)
        m_wantedCastling = pos.castlingRights();
    if (m_auto->isChecked())
        m_position.setSideToMove(automaticSideToMove(m_position));
    else
        m_position.setSideToMove(m_black->isChecked() ? Black : White);
    m_position.setCastlingRights(m_wantedCastling);
    m_board->setPosition(m_position);
    updateControls();
}

void BoardEditorDialog::onBoardEdited(const Position &pos)
{
    const int ep = m_position.enPassantSquare();
    Position p = pos;
    p.setEnPassantSquare(ep);
    setPosition(p, false);
}

void BoardEditorDialog::onSideChanged()
{
    QSettings().setValue(QStringLiteral("editor/side"), m_auto->isChecked() ? 2 : 0);
    setPosition(m_position, false);
}

void BoardEditorDialog::onCastlingChanged()
{
    if (m_updating)
        return;
    const int possible = m_position.possibleCastlingRights();
    for (int i = 0; i < 4; ++i) {
        if (!(possible & kCastlingFlags[i]))
            continue; // disabled boxes keep the wish
        if (m_castling[i]->isChecked())
            m_wantedCastling |= kCastlingFlags[i];
        else
            m_wantedCastling &= ~kCastlingFlags[i];
    }
    setPosition(m_position, false);
}

void BoardEditorDialog::onEnPassantChanged()
{
    m_position.setEnPassantSquare(m_enPassant->currentData().toInt());
    setPosition(m_position, false);
}

void BoardEditorDialog::onMoveNumberChanged()
{
    if (m_updating)
        return;
    m_position.setMoveCounters(m_position.halfmoveClock(), m_moveNumber->value());
    updateControls();
}

void BoardEditorDialog::onFenEdited()
{
    if (m_updating || m_fen->text().trimmed() == m_position.toFen())
        return;
    Position p;
    QString error;
    if (!parseFen(m_fen->text(), &p, &error)) {
        m_status->setText(QStringLiteral("<span style='color:#d03030'>%1</span>").arg(error.toHtmlEscaped()));
        return;
    }
    // the side to move of the FEN counts
    (p.sideToMove() == White ? m_white : m_black)->setChecked(true);
    setPosition(p, true);
}

void BoardEditorDialog::updateControls()
{
    m_updating = true;
    const Color side = m_position.sideToMove();
    m_auto->setText(tr("&Decided by the program (%1)").arg(side == White ? tr("White") : tr("Black")));

    const int possible = m_position.possibleCastlingRights();
    for (int i = 0; i < 4; ++i) {
        m_castling[i]->setEnabled(possible & kCastlingFlags[i]);
        m_castling[i]->setChecked(m_position.castlingRights() & kCastlingFlags[i]);
    }

    // en passant: every square behind a pawn that may just have made a double step
    m_enPassant->clear();
    m_enPassant->addItem(QStringLiteral("-"), NoSquare);
    for (int file = 0; file < 8; ++file) {
        Position test = m_position;
        const int sq = makeSquare(file, side == White ? 5 : 2);
        test.setEnPassantSquare(sq);
        if (test.enPassantSquare() == sq)
            m_enPassant->addItem(squareName(sq), sq);
    }
    m_enPassant->setCurrentIndex(qMax(0, m_enPassant->findData(m_position.enPassantSquare())));
    m_enPassant->setEnabled(m_enPassant->count() > 1);

    m_moveNumber->setValue(m_position.fullmoveNumber());
    m_fen->setText(m_position.toFen());
    m_fen->setCursorPosition(0);

    const QString error = validationError(m_position);
    if (!error.isEmpty()) {
        m_status->setText(QStringLiteral("<span style='color:#d03030'>%1</span>").arg(error.toHtmlEscaped()));
    } else {
        QString text;
        switch (m_position.status()) {
        case GameStatus::Checkmate: text = tr("Valid position: %1 is checkmated.").arg(side == White ? tr("White") : tr("Black")); break;
        case GameStatus::Stalemate: text = tr("Valid position: %1 is stalemated.").arg(side == White ? tr("White") : tr("Black")); break;
        default:
            text = m_position.inCheck() ? tr("Valid position, %1 is in check.").arg(side == White ? tr("White") : tr("Black"))
                                        : tr("Valid position.");
        }
        m_status->setText(text);
    }
    m_apply->setEnabled(error.isEmpty());
    m_updating = false;
}

void BoardEditorDialog::accept()
{
    const QString error = validationError(m_position);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Set up position"), tr("The position cannot be used:\n%1").arg(error));
        return;
    }
    QDialog::accept();
}

void BoardEditorDialog::keyPressEvent(QKeyEvent *event)
{
    // Enter in the FEN line takes the FEN, it does not close the dialog
    if (m_fen->hasFocus() && (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter)) {
        onFenEdited();
        return;
    }
    QDialog::keyPressEvent(event);
}

void BoardEditorDialog::loadPosition()
{
    QSettings s;
    const QString file = QFileDialog::getOpenFileName(
        this, tr("Load position"), s.value(QStringLiteral("editor/dir"), QDir::homePath()).toString(),
        tr("Positions (*.fen *.epd *.txt);;All files (*)"));
    if (file.isEmpty())
        return;
    s.setValue(QStringLiteral("editor/dir"), QFileInfo(file).absolutePath());
    QFile f(file);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Load position"), tr("The file could not be opened:\n%1").arg(f.errorString()));
        return;
    }
    QTextStream in(&f);
    QString line;
    while (!in.atEnd() && line.isEmpty())
        line = in.readLine().trimmed();
    Position p;
    QString error;
    if (!parseFen(line, &p, &error)) {
        QMessageBox::warning(this, tr("Load position"), tr("The file holds no readable position:\n%1").arg(error));
        return;
    }
    (p.sideToMove() == White ? m_white : m_black)->setChecked(true);
    setPosition(p, true);
}

void BoardEditorDialog::savePosition()
{
    QSettings s;
    QString file = QFileDialog::getSaveFileName(
        this, tr("Save position"), s.value(QStringLiteral("editor/dir"), QDir::homePath()).toString(),
        tr("FEN (*.fen)"));
    if (file.isEmpty())
        return;
    if (QFileInfo(file).suffix().isEmpty())
        file += QStringLiteral(".fen");
    s.setValue(QStringLiteral("editor/dir"), QFileInfo(file).absolutePath());
    QSaveFile f(file);
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        f.write(m_position.toFen().toLatin1() + '\n');
        if (f.commit())
            return;
    }
    QMessageBox::warning(this, tr("Save position"), tr("The position could not be saved:\n%1").arg(f.errorString()));
}

void BoardEditorDialog::clearBoard()
{
    Position p;
    p.setMoveCounters(0, m_position.fullmoveNumber());
    m_wantedCastling = 0;
    setPosition(p, false);
}

void BoardEditorDialog::startPosition()
{
    if (!m_auto->isChecked())
        m_white->setChecked(true);
    setPosition(Position::startPosition(), true);
}

void BoardEditorDialog::currentPosition()
{
    if (!m_auto->isChecked())
        (m_current.sideToMove() == White ? m_white : m_black)->setChecked(true);
    setPosition(m_current, true);
}

