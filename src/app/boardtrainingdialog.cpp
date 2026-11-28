// SPDX-License-Identifier: GPL-3.0-or-later
#include "boardtrainingdialog.h"

#include "boardwidget.h"
#include "piecepalette.h"
#include "trainer.h"

#include <chesscore/notation.h>

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QLocale>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

using namespace mbc;

namespace {

const int kQuestions = 20;        // squares in a round
const int kPenaltyMs = 5000;      // for each mistake

const QColor kGood(60, 170, 60, 170);
const QColor kBad(210, 40, 40, 170);
const QColor kAsk(250, 200, 30, 200);
const QColor kCover(120, 120, 120);

struct MemoryLevel
{
    int minPieces, maxPieces, seconds;
};
const MemoryLevel kMemoryLevels[] = { { 3, 5, 5 },    { 6, 8, 8 },    { 9, 12, 12 },
                                      { 13, 18, 20 }, { 19, 24, 30 }, { 25, 32, 40 } };
const int kMemoryLevelCount = int(sizeof(kMemoryLevels) / sizeof(kMemoryLevels[0]));

const char *const kModeKeys[BoardTrainingDialog::ModeCount] = { "findSquare", "nameSquare", "squareColor",
                                                                "memory",     "checksCaptures", "allMoves" };

int pieceCount(const Position &pos)
{
    return popCount(pos.occupancy());
}

bool isLight(int sq)
{
    return (fileOf(sq) + rankOf(sq)) % 2 == 1;
}

// The positions of all puzzles (mate and tactics), as the user sees them at the start of
// the task: real positions from games.
const QVector<Position> &positions()
{
    static QVector<Position> list;
    if (!list.isEmpty())
        return list;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    auto add = [](const QVector<training::Puzzle> &puzzles) {
        for (const training::Puzzle &p : puzzles) {
            Position pos;
            if (!pos.setFromFen(p.fen))
                continue;
            if (!p.firstMove.isEmpty()) {
                const Move m = parseUci(pos, p.firstMove);
                if (m.isNull())
                    continue;
                pos.makeMove(m);
            }
            if (pos.status() == GameStatus::Ongoing)
                list << pos;
        }
    };
    for (int n = 1; n <= 3; ++n)
        add(training::matePuzzles(n));
    add(training::tacticPuzzles());
    QApplication::restoreOverrideCursor();
    return list;
}

// The moves to find: one per from/to (promotions count once).
QVector<Move> targetMoves(const Position &pos, bool checksAndCaptures)
{
    QVector<Move> out;
    for (const Move &m : pos.legalMoves()) {
        if (checksAndCaptures && !pos.isCapture(m) && !pos.after(m).inCheck())
            continue;
        bool seen = false;
        for (const Move &o : out)
            seen = seen || (o.from() == m.from() && o.to() == m.to());
        if (!seen)
            out << m;
    }
    return out;
}

// A random position that fits the exercise.
Position randomPosition(BoardTrainingDialog::Mode mode, int level)
{
    const QVector<Position> &all = positions();
    QVector<int> fitting;
    for (int i = 0; i < all.size(); ++i) {
        const Position &p = all.at(i);
        if (mode == BoardTrainingDialog::Memory) {
            const int n = pieceCount(p);
            if (n >= kMemoryLevels[level].minPieces && n <= kMemoryLevels[level].maxPieces)
                fitting << i;
        } else if (mode == BoardTrainingDialog::ChecksCaptures) {
            const int n = targetMoves(p, true).size();
            if (n >= 3 && n <= 10)
                fitting << i;
        } else {
            const int n = targetMoves(p, false).size();
            if (n >= 5 && n <= 20)
                fitting << i;
        }
    }
    if (fitting.isEmpty())
        return all.isEmpty() ? Position::startPosition() : all.at(QRandomGenerator::global()->bounded(all.size()));
    return all.at(fitting.at(QRandomGenerator::global()->bounded(fitting.size())));
}

QString seconds(qint64 ms)
{
    return QLocale().toString(ms / 1000.0, 'f', 1);
}

} // namespace

BoardTrainingDialog::BoardTrainingDialog(Mode mode, const QString &boardTheme, const QString &pieceSet,
                                         const QString &pieceLetters, QWidget *parent)
    : QDialog(parent, Qt::Window)
    , m_mode(mode)
    , m_pieceLetters(pieceLetters)
{
    setWindowTitle(modeName(mode));
    QSettings settings;

    m_board = new BoardWidget(this);
    m_board->setBoardTheme(boardTheme);
    m_board->setPieceSet(pieceSet);
    m_board->setPosition(Position());
    m_board->setAutoQueen(true);
    m_board->setMovableColors(false, false);
    m_blackPalette = new PiecePalette(Black, m_board, this);
    m_whitePalette = new PiecePalette(White, m_board, this);
    for (PiecePalette *pal : { m_blackPalette, m_whitePalette }) {
        pal->setVisible(mode == Memory); // always there, so that the board keeps its size
        pal->setEnabled(false);
        connect(pal, &PiecePalette::pieceChosen, this, [this](Piece p) {
            m_board->setPlacePiece(p);
            m_blackPalette->setSelected(p);
            m_whitePalette->setSelected(p);
        });
    }
    auto *left = new QVBoxLayout;
    left->addWidget(m_blackPalette);
    left->addWidget(m_board, 1);
    left->addWidget(m_whitePalette);

    m_title = new QLabel(modeName(mode), this);
    QFont tf = m_title->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.4);
    tf.setBold(true);
    m_title->setFont(tf);
    m_description = new QLabel(this);
    m_description->setWordWrap(true);

    auto *right = new QVBoxLayout;
    right->addWidget(m_title);
    right->addWidget(m_description);

    auto *options = new QFormLayout;
    if (mode == FindSquare || mode == NameSquare) {
        m_flip = new QCheckBox(tr("Board seen from &Black"), this);
        m_flip->setChecked(settings.value(QStringLiteral("training/board/flipped"), false).toBool());
        m_coordinates = new QCheckBox(tr("Show &coordinates"), this);
        m_coordinates->setChecked(settings.value(QStringLiteral("training/board/coordinates"), false).toBool());
        options->addRow(m_flip);
        options->addRow(m_coordinates);
        connect(m_flip, &QCheckBox::toggled, this, [this](bool on) {
            m_board->setFlipped(on);
            QSettings().setValue(QStringLiteral("training/board/flipped"), on);
            stopRound();
        });
        connect(m_coordinates, &QCheckBox::toggled, this, [this](bool on) {
            m_board->setShowCoordinates(on);
            QSettings().setValue(QStringLiteral("training/board/coordinates"), on);
            stopRound();
        });
        m_board->setFlipped(m_flip->isChecked());
        m_board->setShowCoordinates(m_coordinates->isChecked());
    }
    if (mode == Memory) {
        m_level = new QComboBox(this);
        for (int i = 0; i < kMemoryLevelCount; ++i)
            m_level->addItem(tr("Level %1: %2 to %3 pieces")
                                 .arg(i + 1)
                                 .arg(kMemoryLevels[i].minPieces)
                                 .arg(kMemoryLevels[i].maxPieces));
        m_seconds = new QSpinBox(this);
        m_seconds->setRange(2, 120);
        m_seconds->setSuffix(tr(" s"));
        const int level = qBound(0, settings.value(QStringLiteral("training/board/memory/level"), 0).toInt(),
                                 kMemoryLevelCount - 1);
        m_level->setCurrentIndex(level);
        m_seconds->setValue(settings.value(QStringLiteral("training/board/memory/seconds%1").arg(level),
                                           kMemoryLevels[level].seconds)
                                .toInt());
        options->addRow(tr("&Level:"), m_level);
        options->addRow(tr("&Time to look:"), m_seconds);
        connect(m_level, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int i) {
            QSettings s;
            s.setValue(QStringLiteral("training/board/memory/level"), i);
            m_seconds->setValue(
                s.value(QStringLiteral("training/board/memory/seconds%1").arg(i), kMemoryLevels[i].seconds).toInt());
            stopRound();
        });
        connect(m_seconds, QOverload<int>::of(&QSpinBox::valueChanged), this, [this](int v) {
            QSettings().setValue(QStringLiteral("training/board/memory/seconds%1").arg(m_level->currentIndex()), v);
        });
    }
    right->addLayout(options);

    m_task = new QLabel(this);
    m_task->setAlignment(Qt::AlignCenter);
    m_task->setWordWrap(true);
    if (isSquareDrill()) {
        QFont f = m_task->font();
        f.setPointSizeF(f.pointSizeF() * 3);
        f.setBold(true);
        m_task->setFont(f);
        m_task->setMinimumHeight(QFontMetrics(f).height() + 8);
    }
    right->addSpacing(8);
    right->addWidget(m_task);

    if (mode == NameSquare) {
        m_name = new QLineEdit(this);
        m_name->setMaxLength(2);
        m_name->setAlignment(Qt::AlignCenter);
        QFont f = m_name->font();
        f.setPointSizeF(f.pointSizeF() * 2);
        m_name->setFont(f);
        m_name->setEnabled(false);
        right->addWidget(m_name);
        connect(m_name, &QLineEdit::textEdited, this, &BoardTrainingDialog::onNameEdited);
    }
    if (mode == SquareColor) {
        auto *row = new QHBoxLayout;
        m_light = new QPushButton(tr("&Light"), this);
        m_dark = new QPushButton(tr("&Dark"), this);
        for (QPushButton *b : { m_light, m_dark }) {
            b->setEnabled(false);
            b->setAutoDefault(false);
            b->setMinimumHeight(b->sizeHint().height() * 2);
            row->addWidget(b);
        }
        right->addLayout(row);
        connect(m_light, &QPushButton::clicked, this, [this] { answer(NoSquare, true); });
        connect(m_dark, &QPushButton::clicked, this, [this] { answer(NoSquare, false); });
    }
    if (isMoveDrill()) {
        m_moves = new QListWidget(this);
        m_moves->setFlow(QListView::LeftToRight);
        m_moves->setWrapping(true);
        m_moves->setResizeMode(QListView::Adjust);
        m_moves->setSpacing(4);
        m_moves->setSelectionMode(QAbstractItemView::NoSelection);
        m_moves->setFocusPolicy(Qt::NoFocus);
        right->addWidget(m_moves, 1);
    }

    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setMinimumHeight(m_status->fontMetrics().height() * 3);
    m_status->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    m_time = new QLabel(this);
    m_best = new QLabel(this);
    m_best->setWordWrap(true);
    right->addWidget(m_status);
    right->addWidget(m_time);
    right->addWidget(m_best);
    if (!isMoveDrill())
        right->addStretch(1);

    m_start = new QPushButton(this);
    m_action = new QPushButton(this);
    m_close = new QPushButton(tr("Close"), this);
    for (QPushButton *b : { m_start, m_action, m_close })
        b->setAutoDefault(false);
    m_start->setDefault(true);
    right->addWidget(m_start);
    right->addWidget(m_action);
    right->addWidget(m_close);
    connect(m_start, &QPushButton::clicked, this, &BoardTrainingDialog::start);
    connect(m_action, &QPushButton::clicked, this, &BoardTrainingDialog::action);
    connect(m_close, &QPushButton::clicked, this, &BoardTrainingDialog::reject);

    auto *rightWidget = new QWidget(this);
    rightWidget->setLayout(right);
    rightWidget->setFixedWidth(qMax(280, rightWidget->sizeHint().width()));
    auto *layout = new QHBoxLayout(this);
    layout->addLayout(left, 1);
    layout->addWidget(rightWidget);

    connect(m_board, &BoardWidget::squareClicked, this, [this](int sq) { answer(sq); });
    connect(m_board, &BoardWidget::moveMade, this, &BoardTrainingDialog::onMove);
    connect(m_board, &BoardWidget::positionEdited, this, [this](const Position &p) { m_rebuilt = p; });

    m_tick = new QTimer(this);
    m_tick->setInterval(100);
    connect(m_tick, &QTimer::timeout, this, &BoardTrainingDialog::updateTime);

    if (mode == SquareColor) {
        m_board->setShowCoordinates(false);
        coverBoard();
    }
    updateTexts();
    resize(900, 620);
}

QString BoardTrainingDialog::modeName(int mode)
{
    switch (mode) {
    case FindSquare: return tr("Find the square");
    case NameSquare: return tr("Name the square");
    case SquareColor: return tr("Light or dark?");
    case Memory: return tr("Remember the position");
    case ChecksCaptures: return tr("All checks and captures");
    case AllMoves: return tr("All moves");
    }
    return QString();
}

QString BoardTrainingDialog::menuText(int mode)
{
    QString name;
    switch (mode) {
    case FindSquare: name = tr("&Find the square"); break;
    case NameSquare: name = tr("&Name the square"); break;
    case SquareColor: name = tr("&Light or dark?"); break;
    case Memory: name = tr("&Remember the position"); break;
    case ChecksCaptures: name = tr("All &checks and captures"); break;
    case AllMoves: name = tr("All &moves"); break;
    default: break;
    }
    QSettings s;
    const QString key = QStringLiteral("training/board/") + QLatin1String(kModeKeys[mode]);
    if (mode <= SquareColor) {
        const qint64 best = s.value(key + QStringLiteral("/best")).toLongLong();
        return best > 0 ? tr("%1 (best %2 s)").arg(name, seconds(best)) : name;
    }
    int solved = 0;
    if (mode == Memory) {
        for (int i = 0; i < kMemoryLevelCount; ++i)
            solved += s.value(key + QStringLiteral("/solved%1").arg(i)).toInt();
    } else {
        solved = s.value(key + QStringLiteral("/solved")).toInt();
    }
    return solved > 0 ? tr("%1 (%n solved)", nullptr, solved).arg(name) : name;
}

void BoardTrainingDialog::resetResults()
{
    QSettings s;
    for (const char *key : kModeKeys)
        s.remove(QStringLiteral("training/board/") + QLatin1String(key));
}

QString BoardTrainingDialog::settingsKey() const
{
    QString key = QStringLiteral("training/board/") + QLatin1String(kModeKeys[m_mode]);
    return key;
}

QString BoardTrainingDialog::san(const Position &pos, Move m) const
{
    return localizeSan(toSan(pos, m), m_pieceLetters);
}

void BoardTrainingDialog::reject()
{
    m_tick->stop();
    QDialog::reject();
}

void BoardTrainingDialog::updateTexts()
{
    const bool running = m_phase != Phase::Idle && m_phase != Phase::Done && m_phase != Phase::Checked;
    switch (m_mode) {
    case FindSquare:
        m_description->setText(tr("A square is named: click it on the board. %1 squares against the clock, "
                                  "each mistake costs 5 seconds.")
                                   .arg(kQuestions));
        break;
    case NameSquare:
        m_description->setText(tr("A square is marked on the board: type its name (for example e4). "
                                  "%1 squares against the clock, each mistake costs 5 seconds.")
                                   .arg(kQuestions));
        break;
    case SquareColor:
        m_description->setText(tr("Without looking at the board: is the named square light or dark? "
                                  "%1 squares against the clock, each mistake costs 5 seconds.")
                                   .arg(kQuestions));
        break;
    case Memory:
        m_description->setText(tr("Look at the position, then set it up again from memory: "
                                  "choose a piece below or above the board and click the squares, "
                                  "or drag the pieces. A right click removes a piece."));
        break;
    case ChecksCaptures:
        m_description->setText(tr("Find every check and every capture of the side to move. Make the moves "
                                  "on the board, they are not played."));
        break;
    case AllMoves:
        m_description->setText(tr("Find every legal move of the side to move. Make the moves on the board, "
                                  "they are not played. A promotion counts once."));
        break;
    default:
        break;
    }

    m_start->setText(running ? tr("&Start again") : tr("&Start"));
    m_action->setVisible(false);
    if (m_mode == Memory) {
        if (m_phase == Phase::Show) {
            m_action->setText(tr("I &have it"));
            m_action->setVisible(true);
        } else if (m_phase == Phase::Rebuild) {
            m_action->setText(tr("&Check"));
            m_action->setVisible(true);
        } else if (m_phase == Phase::Checked) {
            m_action->setVisible(true);
        }
    } else if (isMoveDrill() && m_phase == Phase::Moves) {
        m_action->setText(tr("&Give up and show the rest"));
        m_action->setVisible(true);
    }

    // best result
    QSettings s;
    if (isSquareDrill()) {
        const qint64 best = s.value(settingsKey() + QStringLiteral("/best")).toLongLong();
        m_best->setText(best > 0 ? tr("Best time: %1 s").arg(seconds(best)) : QString());
    } else if (m_mode == Memory) {
        const int level = m_level->currentIndex();
        const int solved = s.value(settingsKey() + QStringLiteral("/solved%1").arg(level)).toInt();
        const int tries = s.value(settingsKey() + QStringLiteral("/tries%1").arg(level)).toInt();
        m_best->setText(tries > 0 ? tr("Level %1: %2 of %3 positions without a mistake")
                                        .arg(level + 1)
                                        .arg(solved)
                                        .arg(tries)
                                  : QString());
    } else {
        const int solved = s.value(settingsKey() + QStringLiteral("/solved")).toInt();
        const int tries = s.value(settingsKey() + QStringLiteral("/tries")).toInt();
        m_best->setText(tries > 0 ? tr("%1 of %2 positions without a mistake").arg(solved).arg(tries) : QString());
    }
}

void BoardTrainingDialog::updateTime()
{
    if (m_phase == Phase::Show) {
        const qint64 left = m_seconds->value() * 1000 - m_clock.elapsed();
        if (left <= 0) {
            startRebuild();
            return;
        }
        m_time->setText(tr("Look: %1 s left").arg((left + 999) / 1000));
        return;
    }
    qint64 ms = m_clock.elapsed();
    if (m_mode == Memory)
        ms -= m_pausedMs;
    m_time->setText(tr("Time: %1 s").arg(seconds(ms)));
}

void BoardTrainingDialog::stopRound()
{
    m_tick->stop();
    m_phase = Phase::Idle;
    m_board->clearMarks();
    m_board->setSquareMode(false);
    m_board->setEditMode(false);
    m_board->setMovableColors(false, false);
    m_board->setLastMove(Move());
    m_blackPalette->setEnabled(false);
    m_whitePalette->setEnabled(false);
    if (m_name) {
        m_name->clear();
        m_name->setEnabled(false);
    }
    if (m_light) {
        m_light->setEnabled(false);
        m_dark->setEnabled(false);
    }
    if (m_mode == SquareColor)
        coverBoard();
    m_task->clear();
    m_status->clear();
    m_time->clear();
    updateTexts();
}

void BoardTrainingDialog::start()
{
    stopRound();
    m_errors = 0;
    m_question = 0;
    m_pausedMs = 0;
    if (isSquareDrill()) {
        m_board->setPosition(Position());
        if (m_mode == SquareColor)
            coverBoard();
        m_board->setSquareMode(m_mode == FindSquare);
        m_clock.start();
        m_tick->start();
        nextQuestion();
    } else if (m_mode == Memory) {
        showMemoryPosition();
    } else {
        startMoves();
    }
    updateTexts();
}

void BoardTrainingDialog::action()
{
    if (m_mode == Memory) {
        if (m_phase == Phase::Show)
            startRebuild();
        else if (m_phase == Phase::Rebuild)
            checkRebuild();
        else if (m_phase == Phase::Checked)
            showOriginal(m_board->position().toFen() != m_original.toFen());
    } else if (isMoveDrill() && m_phase == Phase::Moves) {
        finishMoves(true);
    }
}

// ---------------------------------------------------------------------------
// Square drills

void BoardTrainingDialog::coverBoard()
{
    for (int sq = 0; sq < 64; ++sq)
        m_board->setMark(sq, kCover);
}

void BoardTrainingDialog::nextQuestion()
{
    if (m_question >= kQuestions) {
        finishSquareRound();
        return;
    }
    ++m_question;
    int sq;
    do
        sq = int(QRandomGenerator::global()->bounded(64));
    while (sq == m_square);
    m_square = sq;
    m_phase = Phase::Question;
    m_status->setText(tr("Square %1 of %2").arg(m_question).arg(kQuestions));

    if (m_mode == SquareColor)
        coverBoard();
    else
        m_board->clearMarks();
    if (m_mode == NameSquare) {
        m_task->setText(QStringLiteral("?"));
        m_board->setMark(sq, kAsk);
        m_name->clear();
        m_name->setEnabled(true);
        m_name->setFocus();
    } else {
        m_task->setText(squareName(sq));
    }
    if (m_light) {
        m_light->setEnabled(true);
        m_dark->setEnabled(true);
    }
}

void BoardTrainingDialog::onNameEdited(const QString &text)
{
    if (m_phase != Phase::Question || text.size() < 2)
        return;
    const int sq = parseSquare(text.toLower());
    if (sq == NoSquare) {
        m_status->setText(tr("\"%1\" is not a square.").arg(text));
        m_name->clear();
        return;
    }
    answer(sq);
}

void BoardTrainingDialog::answer(int square, bool light)
{
    if (m_phase != Phase::Question)
        return;
    bool right;
    if (m_mode == SquareColor)
        right = isLight(m_square) == light;
    else
        right = square == m_square;
    m_phase = Phase::Pause;
    if (m_name)
        m_name->setEnabled(false);
    if (m_light) {
        m_light->setEnabled(false);
        m_dark->setEnabled(false);
    }
    if (m_mode == SquareColor) {
        // uncover the square
        m_board->setMark(m_square, QColor());
        m_board->setMark(m_square, right ? kGood : kBad, true);
    } else {
        m_board->setMark(m_square, kGood);
        if (!right && square != NoSquare)
            m_board->setMark(square, kBad);
        m_task->setText(squareName(m_square));
    }
    if (!right) {
        ++m_errors;
        if (m_mode == SquareColor)
            m_status->setText(isLight(m_square) ? tr("Wrong: %1 is a light square.").arg(squareName(m_square))
                                                : tr("Wrong: %1 is a dark square.").arg(squareName(m_square)));
        else if (m_mode == FindSquare)
            m_status->setText(tr("Wrong: that was %1.").arg(squareName(square)));
        else
            m_status->setText(tr("Wrong: the square is %1.").arg(squareName(m_square)));
    }
    QTimer::singleShot(right ? 250 : 1200, this, [this] {
        if (m_phase == Phase::Pause)
            nextQuestion();
    });
}

void BoardTrainingDialog::finishSquareRound()
{
    m_tick->stop();
    m_phase = Phase::Done;
    m_board->setSquareMode(false);
    const qint64 ms = m_clock.elapsed();
    const qint64 total = ms + qint64(m_errors) * kPenaltyMs;
    QString text = m_errors == 0
        ? tr("Done: %1 squares in %2 s without a mistake.").arg(kQuestions).arg(seconds(ms))
        : tr("Done: %1 squares in %2 s, %n mistake(s) (+%3 s): %4 s.", nullptr, m_errors)
              .arg(kQuestions)
              .arg(seconds(ms))
              .arg(m_errors * kPenaltyMs / 1000)
              .arg(seconds(total));
    QSettings s;
    const QString key = settingsKey() + QStringLiteral("/best");
    const qint64 best = s.value(key).toLongLong();
    if (best <= 0 || total < best) {
        s.setValue(key, total);
        if (best > 0)
            text += QLatin1Char(' ') + tr("New best time!");
    }
    m_task->setText(QStringLiteral("\u2713"));
    m_status->setText(text);
    m_time->setText(tr("Time: %1 s").arg(seconds(total)));
    updateTexts();
}

// ---------------------------------------------------------------------------
// Remember the position

void BoardTrainingDialog::showMemoryPosition()
{
    m_original = randomPosition(Memory, m_level->currentIndex());
    m_board->setFlipped(false);
    m_board->setShowCoordinates(true);
    m_board->setPosition(m_original);
    m_phase = Phase::Show;
    m_task->setText(tr("Remember the position (%n piece(s)).", nullptr, pieceCount(m_original)));
    m_clock.start();
    m_tick->start();
    updateTime();
}

void BoardTrainingDialog::startRebuild()
{
    m_pausedMs = m_clock.elapsed();
    m_phase = Phase::Rebuild;
    m_rebuilt = Position();
    m_board->setPosition(m_rebuilt);
    m_board->setEditMode(true);
    m_board->setPlacePiece(NoPiece);
    m_blackPalette->setSelected(NoPiece);
    m_whitePalette->setSelected(NoPiece);
    m_blackPalette->setEnabled(true);
    m_whitePalette->setEnabled(true);
    m_task->setText(tr("Set up the position again (%n piece(s)).", nullptr, pieceCount(m_original)));
    m_status->clear();
    updateTime();
    updateTexts();
}

void BoardTrainingDialog::checkRebuild()
{
    m_tick->stop();
    m_rebuilt.setSideToMove(m_original.sideToMove());
    int right = 0, wrong = 0, missing = 0;
    m_board->clearMarks();
    for (int sq = 0; sq < 64; ++sq) {
        const Piece want = m_original.pieceAt(sq);
        const Piece have = m_rebuilt.pieceAt(sq);
        if (want == NoPiece && have == NoPiece)
            continue;
        if (want == have) {
            ++right;
        } else if (have == NoPiece) {
            ++missing;
        } else {
            ++wrong;
            if (want != NoPiece)
                ++missing;
        }
    }
    m_phase = Phase::Checked;
    m_board->setEditMode(false);
    m_blackPalette->setSelected(NoPiece);
    m_whitePalette->setSelected(NoPiece);
    m_blackPalette->setEnabled(false);
    m_whitePalette->setEnabled(false);

    const bool solved = wrong == 0 && missing == 0;
    QSettings s;
    const int level = m_level->currentIndex();
    const QString key = settingsKey();
    s.setValue(key + QStringLiteral("/tries%1").arg(level), s.value(key + QStringLiteral("/tries%1").arg(level)).toInt() + 1);
    if (solved)
        s.setValue(key + QStringLiteral("/solved%1").arg(level),
                   s.value(key + QStringLiteral("/solved%1").arg(level)).toInt() + 1);

    const int total = pieceCount(m_original);
    if (solved) {
        m_task->setText(tr("Correct!"));
        m_status->setText(tr("All %n piece(s) are on the right squares.", nullptr, total));
    } else {
        m_task->setText(tr("%1 of %2 pieces right").arg(right).arg(total));
        QStringList parts;
        if (wrong)
            parts << tr("%n piece(s) on a wrong square (red)", nullptr, wrong);
        if (missing)
            parts << tr("%n piece(s) missing (red frame)", nullptr, missing);
        m_status->setText(parts.join(QStringLiteral(", ")) + QLatin1Char('.'));
    }
    showOriginal(false);
    updateTexts();
}

// Checked: the user's position with marks, or the original.
void BoardTrainingDialog::showOriginal(bool original)
{
    m_board->clearMarks();
    if (original) {
        m_board->setPosition(m_original);
        m_action->setText(tr("Show &my position"));
        return;
    }
    m_board->setPosition(m_rebuilt);
    for (int sq = 0; sq < 64; ++sq) {
        const Piece want = m_original.pieceAt(sq);
        const Piece have = m_rebuilt.pieceAt(sq);
        if (have != NoPiece)
            m_board->setMark(sq, have == want ? kGood : kBad);
        else if (want != NoPiece)
            m_board->setMark(sq, kBad, true);
    }
    m_action->setText(tr("Show the &original"));
}

// ---------------------------------------------------------------------------
// Checks and captures, all moves

void BoardTrainingDialog::startMoves()
{
    m_original = randomPosition(m_mode, 0);
    m_targets = targetMoves(m_original, m_mode == ChecksCaptures);
    m_found.clear();
    m_moves->clear();
    m_board->setFlipped(m_original.sideToMove() == Black);
    m_board->setShowCoordinates(true);
    m_board->setPosition(m_original);
    m_board->setMovableColors(m_original.sideToMove() == White, m_original.sideToMove() == Black);
    m_phase = Phase::Moves;
    const QString side = m_original.sideToMove() == White ? tr("White to move") : tr("Black to move");
    m_task->setText(side + QStringLiteral(": ")
                    + (m_mode == ChecksCaptures ? tr("find all %n check(s) and capture(s).", nullptr, m_targets.size())
                                                : tr("find all %n move(s).", nullptr, m_targets.size())));
    m_status->setText(tr("Found: %1 of %2").arg(0).arg(m_targets.size()));
    m_clock.start();
    m_tick->start();
}

void BoardTrainingDialog::onMove(Move move)
{
    if (m_phase != Phase::Moves)
        return;
    int index = -1;
    for (int i = 0; i < m_targets.size(); ++i)
        if (m_targets.at(i).from() == move.from() && m_targets.at(i).to() == move.to())
            index = i;
    const QString text = san(m_original, index >= 0 ? m_targets.at(index) : move);
    QString note;
    if (index < 0) {
        ++m_errors;
        note = tr("%1 is neither a check nor a capture.").arg(text);
    } else if (m_found.contains(index)) {
        note = tr("%1 was found already.").arg(text);
    } else {
        m_found.insert(index);
        auto *item = new QListWidgetItem(text, m_moves);
        item->setForeground(QColor(30, 130, 30));
        m_board->setLastMove(move);
    }
    QString status = tr("Found: %1 of %2").arg(m_found.size()).arg(m_targets.size());
    if (m_errors)
        status += QStringLiteral(", ") + tr("%n mistake(s)", nullptr, m_errors);
    if (!note.isEmpty())
        status += QLatin1Char('\n') + note;
    m_status->setText(status);
    if (m_found.size() == m_targets.size())
        finishMoves(false);
}

void BoardTrainingDialog::finishMoves(bool gaveUp)
{
    m_tick->stop();
    m_phase = Phase::Done;
    m_board->setMovableColors(false, false);
    m_board->setLastMove(Move());
    int missing = 0;
    for (int i = 0; i < m_targets.size(); ++i) {
        if (m_found.contains(i))
            continue;
        ++missing;
        auto *item = new QListWidgetItem(san(m_original, m_targets.at(i)), m_moves);
        item->setForeground(QColor(200, 30, 30));
        m_board->setMark(m_targets.at(i).to(), kBad, true);
    }
    const bool solved = !gaveUp && m_errors == 0;
    QSettings s;
    s.setValue(settingsKey() + QStringLiteral("/tries"), s.value(settingsKey() + QStringLiteral("/tries")).toInt() + 1);
    if (solved)
        s.setValue(settingsKey() + QStringLiteral("/solved"),
                   s.value(settingsKey() + QStringLiteral("/solved")).toInt() + 1);
    const QString time = seconds(m_clock.elapsed());
    if (missing)
        m_status->setText(tr("Found %1 of %2. The missing moves are red.").arg(m_found.size()).arg(m_targets.size()));
    else if (m_errors)
        m_status->setText(tr("All found in %1 s, %n mistake(s).", nullptr, m_errors).arg(time));
    else
        m_status->setText(tr("All found in %1 s without a mistake!").arg(time));
    updateTexts();
}
