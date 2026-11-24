// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - board editor ("set up position"): piece palettes above (Black) and below
// (White) the board, pieces placed by drag & drop or click, side to move, castling
// rights, en passant, move number, FEN line, load/save as .fen.
#pragma once

#include <chesscore/position.h>

#include <QDialog>

class BoardWidget;
class PiecePalette;
class QButtonGroup;
class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QSpinBox;

class BoardEditorDialog : public QDialog
{
    Q_OBJECT
public:
    BoardEditorDialog(const mbc::Position &start, const QString &boardTheme, const QString &pieceSet,
                      QWidget *parent = nullptr, const mbc::Position *current = nullptr);
    // Opens with an empty board; the button "Current position" brings back `current`.
    BoardEditorDialog(const mbc::Position &start, const mbc::Position &current, const QString &boardTheme,
                      const QString &pieceSet, QWidget *parent = nullptr);

    mbc::Position position() const { return m_position; }

    // The side to move chosen by "decided by the program": the side in check, else White.
    static mbc::Color automaticSideToMove(const mbc::Position &pos);
    // Reads a FEN or EPD leniently (missing kings are allowed, missing fields get
    // defaults). False and *error for a broken piece placement.
    static bool parseFen(const QString &text, mbc::Position *pos, QString *error);
    // Rules for a position to play with; empty if it is fine.
    static QString validationError(const mbc::Position &pos);

protected:
    void accept() override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void setPosition(const mbc::Position &pos, bool takeCastling);
    void onBoardEdited(const mbc::Position &pos);
    void onSideChanged();
    void onCastlingChanged();
    void onEnPassantChanged();
    void onMoveNumberChanged();
    void onFenEdited();
    void loadPosition();
    void savePosition();
    void clearBoard();
    void startPosition();
    void currentPosition();
    void updateControls();

    mbc::Position m_position;
    mbc::Position m_current;     // the game's position for "Current position"
    bool m_hasCurrent = false;
    int m_wantedCastling = 0;    // the user's choice; effective only with king and rook in place
    bool m_updating = false;

    BoardWidget *m_board;
    PiecePalette *m_blackPalette;
    PiecePalette *m_whitePalette;
    QRadioButton *m_white;
    QRadioButton *m_black;
    QRadioButton *m_auto;
    QButtonGroup *m_sideGroup;
    QCheckBox *m_castling[4];
    QComboBox *m_enPassant;
    QSpinBox *m_moveNumber;
    QLineEdit *m_fen;
    QLabel *m_status;
    QPushButton *m_apply;
};
