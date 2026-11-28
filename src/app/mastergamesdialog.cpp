// SPDX-License-Identifier: GPL-3.0-or-later
#include "mastergamesdialog.h"

#include "boardwidget.h"
#include "engineanalysis.h"
#include "languagemanager.h"
#include "tutor.h"

#include <chesscore/notation.h>
#include <pgn/pgn.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QComboBox>
#include <QSettings>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

using namespace mbc;

namespace {

const int kMasterPoints = 10;
const int kReplayMs = 700;        // between the moves the user does not guess
const int kGiveUpMs = 15000;      // the engine does not answer: the guess gets no points

const QColor kHint(250, 200, 30, 200);

// Points for a move that is not the master's: by the winning chances it loses against it
int pointsForDrop(double drop)
{
    if (drop <= 1)
        return 8;
    if (drop <= 3)
        return 6;
    if (drop <= 6)
        return 4;
    if (drop <= 10)
        return 2;
    if (drop <= 15)
        return 1;
    return 0;
}

QLocale uiLocale()
{
    return QLocale(LanguageManager::savedLanguage() == QLatin1String("de") ? QLocale::German : QLocale::English);
}

// Mate and stalemate need no engine
bool terminalEval(const Position &pos, Evaluation *e)
{
    if (!pos.legalMoves().empty())
        return false;
    e->valid = true;
    e->mate = false;
    e->value = pos.isCheckmate() ? (pos.sideToMove() == White ? -100000 : 100000) : 0;
    return true;
}

} // namespace

QString MasterGamesDialog::builtinFile()
{
    return QStringLiteral(":/training/masters/classics.pgn");
}

MasterGamesDialog::MasterGamesDialog(const EngineEntry &engine, const QString &boardTheme, const QString &pieceSet,
                                     const QString &pieceLetters, QWidget *parent)
    : QDialog(parent, Qt::Window)
    , m_pieceLetters(pieceLetters)
    , m_engine(engine)
    , m_poll(new QTimer(this))
    , m_replay(new QTimer(this))
{
    setWindowTitle(tr("Master games"));
    QSettings settings;

    m_poll->setInterval(100);
    connect(m_poll, &QTimer::timeout, this, &MasterGamesDialog::poll);
    m_replay->setSingleShot(true);
    m_replay->setInterval(kReplayMs);
    connect(m_replay, &QTimer::timeout, this, &MasterGamesDialog::step);
    if (m_engine.isValid()) {
        m_analysis = new EngineAnalysis(this);
        m_analysis->setMultiPv(1);
        connect(m_analysis, &EngineAnalysis::errorOccurred, this, [this](const QString &message) {
            m_status->setText(tr("The engine does not work (%1); other moves than the master's get no points.")
                                  .arg(message));
            m_analysis->deleteLater();
            m_analysis = nullptr;
            if (m_phase == Phase::Rating) {
                m_poll->stop();
                scoreGuess(0, false, QString());
            }
        });
    }

    m_board = new BoardWidget(this);
    m_board->setBoardTheme(boardTheme);
    m_board->setPieceSet(pieceSet);
    m_board->setPosition(Position::startPosition());
    m_board->setAutoQueen(false);
    m_board->setMovableColors(false, false);
    connect(m_board, &BoardWidget::moveMade, this, &MasterGamesDialog::onMove);

    auto *title = new QLabel(tr("Master games"), this);
    QFont tf = title->font();
    tf.setPointSizeF(tf.pointSizeF() * 1.4);
    tf.setBold(true);
    title->setFont(tf);
    auto *description = new QLabel(tr("Guess the moves of a master. The master's move gives %1 points, "
                                      "another move up to 8, as the engine rates it against the master's move. "
                                      "A hint shows the piece and halves the points.")
                                       .arg(kMasterPoints),
                                   this);
    description->setWordWrap(true);

    m_games = new QComboBox(this);
    m_games->setSizeAdjustPolicy(QComboBox::AdjustToMinimumContentsLengthWithIcon);
    m_games->setMinimumContentsLength(24);
    m_file = new QPushButton(tr("&File..."), this);
    auto *gameRow = new QHBoxLayout;
    gameRow->addWidget(m_games, 1);
    gameRow->addWidget(m_file);
    m_side = new QComboBox(this);
    m_side->addItem(tr("White"));
    m_side->addItem(tr("Black"));
    m_fromMove = new QSpinBox(this);
    m_fromMove->setRange(1, 1);
    m_seconds = new QSpinBox(this);
    m_seconds->setRange(1, 30);
    m_seconds->setSuffix(tr(" s"));
    m_seconds->setValue(settings.value(QStringLiteral("training/masters/seconds"), 2).toInt());
    m_seconds->setEnabled(m_engine.isValid());

    auto *options = new QFormLayout;
    auto *gameLabel = new QLabel(tr("&Game:"), this);
    gameLabel->setBuddy(m_games);
    options->addRow(gameLabel, gameRow);
    options->addRow(tr("&Play as:"), m_side);
    options->addRow(tr("Guess &from move:"), m_fromMove);
    options->addRow(tr("&Engine time per move:"), m_seconds);

    m_info = new QLabel(this);
    m_info->setWordWrap(true);
    m_task = new QLabel(this);
    m_task->setWordWrap(true);
    QFont f = m_task->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 1.2);
    m_task->setFont(f);
    m_status = new QLabel(this);
    m_status->setWordWrap(true);
    m_status->setTextFormat(Qt::RichText);
    m_list = new QListWidget(this);
    m_list->setMinimumHeight(120);
    m_score = new QLabel(this);
    m_best = new QLabel(this);

    m_start = new QPushButton(tr("&Start"), this);
    m_hint = new QPushButton(tr("&Hint"), this);
    m_show = new QPushButton(tr("Show the &move"), this);
    m_close = new QPushButton(tr("&Close"), this);
    m_start->setDefault(true);
    connect(m_start, &QPushButton::clicked, this, &MasterGamesDialog::start);
    connect(m_hint, &QPushButton::clicked, this, &MasterGamesDialog::hint);
    connect(m_show, &QPushButton::clicked, this, &MasterGamesDialog::showMove);
    connect(m_close, &QPushButton::clicked, this, &MasterGamesDialog::reject);
    auto *buttons = new QHBoxLayout;
    buttons->addWidget(m_start);
    buttons->addWidget(m_hint);
    buttons->addWidget(m_show);
    buttons->addStretch();
    buttons->addWidget(m_close);

    auto *right = new QVBoxLayout;
    right->addWidget(title);
    right->addWidget(description);
    right->addLayout(options);
    right->addWidget(m_info);
    right->addSpacing(6);
    right->addWidget(m_task);
    right->addWidget(m_status);
    right->addWidget(m_list, 1);
    right->addWidget(m_score);
    right->addWidget(m_best);
    if (!m_engine.isValid()) {
        auto *note = new QLabel(tr("No analysis engine: only the master's move gives points."), this);
        note->setWordWrap(true);
        right->addWidget(note);
    }
    right->addLayout(buttons);

    auto *layout = new QHBoxLayout(this);
    layout->addWidget(m_board, 3);
    layout->addLayout(right, 2);
    resize(1000, 640);

    connect(m_file, &QPushButton::clicked, this, &MasterGamesDialog::chooseFile);
    connect(m_games, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MasterGamesDialog::onGameChosen);
    connect(m_side, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this] {
        stopRound();
        updateBest();
    });
    connect(m_seconds, QOverload<int>::of(&QSpinBox::valueChanged), this,
            [](int v) { QSettings().setValue(QStringLiteral("training/masters/seconds"), v); });

    QString path = settings.value(QStringLiteral("training/masters/file")).toString();
    if (path.isEmpty() || (!path.startsWith(QLatin1Char(':')) && !QFileInfo::exists(path)))
        path = builtinFile();
    loadFile(path);
    if (m_gameList.isEmpty() && path != builtinFile())
        loadFile(builtinFile());
    const int game = settings.value(QStringLiteral("training/masters/game"), 0).toInt();
    if (game > 0 && game < m_games->count())
        m_games->setCurrentIndex(game);
    updateButtons();
}

MasterGamesDialog::~MasterGamesDialog()
{
    if (m_analysis)
        m_analysis->stop();
}

void MasterGamesDialog::reject()
{
    m_poll->stop();
    m_replay->stop();
    if (m_analysis)
        m_analysis->stop();
    QDialog::reject();
}

void MasterGamesDialog::loadFile(const QString &path)
{
    QVector<Game> games;
    QString error;
    if (!pgn::readFile(path, &games, &error)) {
        QMessageBox::warning(this, tr("Master games"), tr("The file cannot be read: %1").arg(error));
        return;
    }
    QVector<Game> usable;
    for (const Game &g : games)
        if (!g.mainLine().isEmpty())
            usable << g;
    if (usable.isEmpty()) {
        QMessageBox::warning(this, tr("Master games"), tr("The file holds no games with moves."));
        return;
    }
    stopRound();
    m_path = path;
    m_gameList = usable;
    QSettings().setValue(QStringLiteral("training/masters/file"), path);
    const QSignalBlocker block(m_games);
    m_games->clear();
    for (const Game &g : m_gameList)
        m_games->addItem(gameName(g));
    m_games->setToolTip(path.startsWith(QLatin1Char(':')) ? tr("Famous games, built in") : path);
    m_games->setCurrentIndex(0);
    onGameChosen(0);
}

void MasterGamesDialog::chooseFile()
{
    QSettings s;
    QString dir = s.value(QStringLiteral("files/lastDir"), QDir::homePath()).toString();
    const QString file = QFileDialog::getOpenFileName(this, tr("Games of masters"), dir,
                                                      tr("PGN files (*.pgn);;All files (*)"));
    if (file.isEmpty())
        return;
    s.setValue(QStringLiteral("files/lastDir"), QFileInfo(file).absolutePath());
    loadFile(file);
    updateButtons();
}

QString MasterGamesDialog::gameName(const Game &g) const
{
    const QString name = g.tag(QStringLiteral("Name_") + LanguageManager::savedLanguage());
    if (!name.isEmpty())
        return name;
    QString text = g.tag(QStringLiteral("White")) + QStringLiteral(" - ") + g.tag(QStringLiteral("Black"));
    const QString event = g.tag(QStringLiteral("Event"));
    const QString year = g.tag(QStringLiteral("Date")).left(4);
    QStringList extra;
    if (!event.isEmpty() && event != QLatin1String("?"))
        extra << event;
    if (!year.isEmpty() && !year.contains(QLatin1Char('?')))
        extra << year;
    if (!extra.isEmpty())
        text += QStringLiteral(", ") + extra.join(QLatin1Char(' '));
    return text;
}

void MasterGamesDialog::onGameChosen(int index)
{
    stopRound();
    if (index < 0 || index >= m_gameList.size())
        return;
    QSettings().setValue(QStringLiteral("training/masters/game"), index);
    const Game &g = m_gameList.at(index);
    // the side of the winner, else White
    const QSignalBlocker block(m_side);
    m_side->setCurrentIndex(g.result() == Result::BlackWins ? 1 : 0);
    const int plies = g.mainLine().size();
    const int fullMoves = g.startPosition().fullmoveNumber() + plies / 2;
    m_fromMove->setRange(g.startPosition().fullmoveNumber(), qMax(g.startPosition().fullmoveNumber(), fullMoves));
    m_fromMove->setValue(g.startPosition().fullmoveNumber());
    QStringList info;
    info << QStringLiteral("<b>%1</b> – <b>%2</b>")
                .arg(g.tag(QStringLiteral("White")).toHtmlEscaped(), g.tag(QStringLiteral("Black")).toHtmlEscaped());
    QStringList where;
    for (const char *t : { "Event", "Site", "Date" }) {
        QString v = g.tag(QLatin1String(t));
        if (qstrcmp(t, "Date") == 0) {
            // "1910.??.??" -> "1910"
            const QStringList parts = v.split(QLatin1Char('.'));
            QStringList known;
            for (const QString &part : parts) {
                if (part.contains(QLatin1Char('?')))
                    break;
                known << part;
            }
            v = known.join(QLatin1Char('.'));
        }
        if (!v.isEmpty() && v != QLatin1String("?") && !where.contains(v.toHtmlEscaped()))
            where << v.toHtmlEscaped();
    }
    if (!where.isEmpty())
        info << where.join(QStringLiteral(", "));
    info << tr("%n moves", nullptr, (plies + 1) / 2) + QStringLiteral(", ") + resultToString(g.result());
    m_info->setText(info.join(QStringLiteral("<br>")));
    m_board->setFlipped(m_side->currentIndex() == 1);
    m_board->setPosition(g.startPosition());
    m_board->setLastMove(Move::null());
    m_board->clearMarks();
    updateBest();
}

QString MasterGamesDialog::settingsKey() const
{
    const int index = m_games->currentIndex();
    QString id = m_path + QLatin1Char('|') + QString::number(index);
    if (index >= 0 && index < m_gameList.size())
        id += QLatin1Char('|') + m_gameList.at(index).uciMoves().join(QLatin1Char(' ')).left(200);
    const QByteArray hash = QCryptographicHash::hash(id.toUtf8(), QCryptographicHash::Sha1).toHex().left(16);
    return QStringLiteral("training/masters/best/") + QString::fromLatin1(hash)
        + (m_side->currentIndex() == 1 ? QStringLiteral("/black") : QStringLiteral("/white"));
}

void MasterGamesDialog::updateBest()
{
    const int best = QSettings().value(settingsKey(), -1).toInt();
    m_best->setText(best < 0 ? QString() : tr("Your best result with this game: %1 %").arg(best));
}

void MasterGamesDialog::updateButtons()
{
    const bool running = m_phase == Phase::Replay || m_phase == Phase::Guess || m_phase == Phase::Rating;
    m_start->setText(running ? tr("&Start again") : tr("&Start"));
    m_start->setEnabled(!m_gameList.isEmpty());
    m_hint->setEnabled(m_phase == Phase::Guess && !m_hinted);
    m_show->setEnabled(m_phase == Phase::Guess);
    m_games->setEnabled(!running);
    m_file->setEnabled(!running);
    m_side->setEnabled(!running);
    m_fromMove->setEnabled(!running);
}

void MasterGamesDialog::stopRound()
{
    m_poll->stop();
    m_replay->stop();
    if (m_analysis && m_analysis->isRunning())
        m_analysis->pause();
    m_phase = Phase::Idle;
    if (m_board)
        m_board->setMovableColors(false, false);
    if (m_task) {
        m_task->clear();
        m_status->clear();
        m_list->clear();
        m_score->clear();
    }
    if (m_start)
        updateButtons();
}

bool MasterGamesDialog::userPly(int ply) const
{
    return ply >= m_firstPly && ply < m_moves.size() && m_positions.at(ply).sideToMove() == m_userColor;
}

void MasterGamesDialog::start()
{
    const int index = m_games->currentIndex();
    if (index < 0 || index >= m_gameList.size())
        return;
    stopRound();
    const Game &g = m_gameList.at(index);
    m_userColor = m_side->currentIndex() == 1 ? Black : White;
    m_moves.clear();
    m_positions.clear();
    Position pos = g.startPosition();
    for (Game::NodeId id : g.mainLine()) {
        m_positions << pos;
        m_moves << g.nodeMove(id);
        pos = g.nodePosition(id);
    }
    m_positions << pos;
    // the first ply of the user from the chosen move on
    m_firstPly = int(m_moves.size());
    for (int i = 0; i < m_moves.size(); ++i) {
        if (m_positions.at(i).fullmoveNumber() >= m_fromMove->value() && m_positions.at(i).sideToMove() == m_userColor) {
            m_firstPly = i;
            break;
        }
    }
    if (m_firstPly >= m_moves.size()) {
        m_status->setText(tr("From this move on there is no move of this side to guess."));
        return;
    }
    m_points = m_maxPoints = m_guessed = 0;
    m_board->setFlipped(m_userColor == Black);
    // the moves before are set up at once
    m_ply = qMax(0, m_firstPly - 1);
    m_board->setPosition(m_positions.at(m_ply));
    m_board->setLastMove(m_ply > 0 ? m_moves.at(m_ply - 1) : Move::null());
    m_board->clearMarks();
    if (m_analysis)
        m_analysis->start(m_engine, 1, 64);
    m_phase = Phase::Replay;
    m_score->setText(tr("Points: %1 of %2").arg(0).arg(0));
    updateButtons();
    if (m_ply < m_firstPly)
        m_replay->start(); // the opponent's move before the first guess
    else
        step();
}

void MasterGamesDialog::step()
{
    if (m_phase != Phase::Replay)
        return;
    if (m_ply >= m_moves.size()) {
        finishGame();
        return;
    }
    if (!userPly(m_ply)) {
        // the other side's move (or one before the first guess)
        m_board->setPosition(m_positions.at(m_ply + 1));
        m_board->setLastMove(m_moves.at(m_ply));
        ++m_ply;
        if (m_ply >= m_moves.size()) {
            finishGame();
            return;
        }
        if (userPly(m_ply))
            step();
        else
            m_replay->start();
        return;
    }
    m_phase = Phase::Guess;
    m_hinted = false;
    m_board->clearMarks();
    m_board->setPosition(m_positions.at(m_ply));
    m_board->setMovableColors(m_userColor == White, m_userColor == Black);
    m_task->setText(tr("%1 What did the master play?").arg(moveNumber(m_ply)));
    if (m_analysis)
        m_analysis->pause();
    updateButtons();
}

QString MasterGamesDialog::moveNumber(int ply) const
{
    const Position &p = m_positions.at(ply);
    return p.sideToMove() == White ? QStringLiteral("%1.").arg(p.fullmoveNumber())
                                   : QStringLiteral("%1...").arg(p.fullmoveNumber());
}

QString MasterGamesDialog::san(const Position &pos, Move m) const
{
    return localizeSan(toSan(pos, m), m_pieceLetters);
}

QString MasterGamesDialog::evalText(const Evaluation &e) const
{
    if (!e.valid)
        return QStringLiteral("?");
    const int v = m_userColor == White ? e.value : -e.value;
    if (e.mate)
        return v > 0 ? QStringLiteral("#%1").arg(v) : QStringLiteral("#-%1").arg(-v);
    if (qAbs(v) >= 100000)
        return v > 0 ? tr("mate") : tr("mated");
    const QString number = uiLocale().toString(qAbs(v) / 100.0, 'f', 1);
    return (v > 0 ? QStringLiteral("+") : v < 0 ? QStringLiteral("−") : QString()) + number;
}

void MasterGamesDialog::onMove(Move move)
{
    if (m_phase != Phase::Guess)
        return;
    m_board->setMovableColors(false, false);
    const Position &before = m_positions.at(m_ply);
    const Move master = m_moves.at(m_ply);
    if (move == master) {
        ++m_guessed;
        scoreGuess(m_hinted ? kMasterPoints / 2 : kMasterPoints, true, QString());
        return;
    }
    m_guess = move;
    // the guess stays on the board while the engine rates it
    m_board->setPosition(before.after(move));
    m_board->setLastMove(move);
    m_guessEval = Evaluation();
    m_masterEval = Evaluation();
    terminalEval(before.after(move), &m_guessEval);
    terminalEval(before.after(master), &m_masterEval);
    if (!m_analysis || (m_guessEval.valid && m_masterEval.valid)) {
        if (m_guessEval.valid && m_masterEval.valid) {
            const double drop = Tutor::winChance(m_masterEval, m_userColor) - Tutor::winChance(m_guessEval, m_userColor);
            int points = pointsForDrop(drop);
            scoreGuess(m_hinted ? points / 2 : points, false, QString());
        } else {
            scoreGuess(0, false, QString());
        }
        return;
    }
    m_phase = Phase::Rating;
    m_status->setText(tr("<b>%1</b> is not the master's move. The engine looks at it...").arg(san(before, move)));
    m_ratingStep = m_guessEval.valid ? 1 : 0;
    QStringList uci;
    for (int i = 0; i < m_ply; ++i)
        uci << toUci(m_moves.at(i));
    m_analysis->analyse(m_positions.constFirst(), uci + QStringList { toUci(m_ratingStep == 0 ? move : master) });
    m_searchClock.start();
    m_poll->start();
    updateButtons();
}

void MasterGamesDialog::poll()
{
    if (m_phase != Phase::Rating || !m_analysis)
        return;
    const QVector<EngineAnalysis::Line> &lines = m_analysis->lines();
    const int wanted = m_seconds->value() * 1000;
    if (m_searchClock.elapsed() > wanted + kGiveUpMs) {
        m_poll->stop();
        scoreGuess(0, false, tr("The engine did not answer."));
        return;
    }
    if (m_searchClock.elapsed() < wanted || lines.isEmpty() || !lines.constFirst().eval.valid)
        return;
    if (m_ratingStep == 0) {
        m_guessEval = lines.constFirst().eval;
        if (!m_masterEval.valid) {
            m_ratingStep = 1;
            QStringList uci;
            for (int i = 0; i < m_ply; ++i)
                uci << toUci(m_moves.at(i));
            m_analysis->analyse(m_positions.constFirst(), uci + QStringList { toUci(m_moves.at(m_ply)) });
            m_searchClock.start();
            return;
        }
    } else {
        m_masterEval = lines.constFirst().eval;
    }
    m_poll->stop();
    const double drop = Tutor::winChance(m_masterEval, m_userColor) - Tutor::winChance(m_guessEval, m_userColor);
    const int points = pointsForDrop(drop);
    scoreGuess(m_hinted ? points / 2 : points, false, QString());
}

void MasterGamesDialog::scoreGuess(int points, bool found, const QString &note)
{
    const Position &before = m_positions.at(m_ply);
    const Move master = m_moves.at(m_ply);
    QString line;
    QString listText;
    if (found) {
        line = tr("<b>%1</b> is the master's move!").arg(san(before, master)) + QLatin1Char(' ')
            + tr("%n points.", nullptr, points);
        listText = tr("%1 %2: found (%3)").arg(moveNumber(m_ply), san(before, master)).arg(points);
    } else {
        const QString guess = san(before, m_guess);
        const QString rating = m_guessEval.valid && m_masterEval.valid
            ? QLatin1Char(' ') + tr("(engine: %1 %2, %3 %4)").arg(guess, evalText(m_guessEval), san(before, master), evalText(m_masterEval))
            : QString();
        line = tr("You played <b>%1</b>, the master <b>%2</b>").arg(guess, san(before, master)) + rating
            + QStringLiteral(". ") + tr("%n points.", nullptr, points) + (note.isEmpty() ? QString() : QLatin1Char(' ') + note);
        listText = tr("%1 %2: you %3 (%4)").arg(moveNumber(m_ply), san(before, master), guess).arg(points);
    }
    m_guess = Move::null();
    m_points += points;
    m_maxPoints += kMasterPoints;
    m_status->setText(line);
    auto *item = new QListWidgetItem(listText, m_list);
    item->setForeground(points >= kMasterPoints / 2 ? QColor(40, 140, 40) : points > 0 ? QColor(190, 130, 0)
                                                                                       : QColor(200, 40, 40));
    m_list->scrollToItem(item);
    m_score->setText(tr("Points: %1 of %2").arg(m_points).arg(m_maxPoints));
    playMasterMove();
}

void MasterGamesDialog::playMasterMove()
{
    m_board->clearMarks();
    m_board->setPosition(m_positions.at(m_ply + 1));
    m_board->setLastMove(m_moves.at(m_ply));
    ++m_ply;
    m_phase = Phase::Replay;
    updateButtons();
    if (m_ply >= m_moves.size())
        finishGame();
    else
        m_replay->start(kReplayMs * 2); // time to see the master's move
}

void MasterGamesDialog::hint()
{
    if (m_phase != Phase::Guess || m_hinted)
        return;
    m_hinted = true;
    m_board->setMark(m_moves.at(m_ply).from(), kHint);
    m_status->setText(tr("The master moves this piece. The move now gives half the points."));
    updateButtons();
}

void MasterGamesDialog::showMove()
{
    if (m_phase != Phase::Guess)
        return;
    m_board->setMovableColors(false, false);
    m_guess = Move::null();
    const Position &before = m_positions.at(m_ply);
    m_maxPoints += kMasterPoints;
    const QString master = san(before, m_moves.at(m_ply));
    m_status->setText(tr("The master played <b>%1</b>. No points.").arg(master));
    auto *item = new QListWidgetItem(tr("%1 %2: shown (0)").arg(moveNumber(m_ply), master), m_list);
    item->setForeground(QColor(200, 40, 40));
    m_list->scrollToItem(item);
    m_score->setText(tr("Points: %1 of %2").arg(m_points).arg(m_maxPoints));
    playMasterMove();
}

void MasterGamesDialog::finishGame()
{
    m_poll->stop();
    m_replay->stop();
    if (m_analysis && m_analysis->isRunning())
        m_analysis->pause();
    m_phase = Phase::Done;
    m_board->setMovableColors(false, false);
    const int percent = m_maxPoints > 0 ? qRound(100.0 * m_points / m_maxPoints) : 0;
    const Game &g = m_gameList.at(m_games->currentIndex());
    m_task->setText(tr("The game is over: %1.").arg(resultToString(g.result())));
    QString text = tr("You have %1 of %2 points (%3 %) and found %4 of %n master moves.", nullptr,
                      m_maxPoints / kMasterPoints)
                       .arg(m_points)
                       .arg(m_maxPoints)
                       .arg(percent)
                       .arg(m_guessed);
    QSettings s;
    const int best = s.value(settingsKey(), -1).toInt();
    if (m_maxPoints > 0 && percent > best) {
        s.setValue(settingsKey(), percent);
        if (best >= 0)
            text += QLatin1Char(' ') + tr("A new best result!");
    }
    m_status->setText(text);
    updateBest();
    updateButtons();
}
