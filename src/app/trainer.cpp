// SPDX-License-Identifier: GPL-3.0-or-later
#include "trainer.h"

#include "datafolders.h"
#include "enginemanager.h"
#include "gamemanager.h"
#include "languagemanager.h"
#include "opponents.h"

#include <chesscore/kpk.h>
#include <chesscore/notation.h>
#include <gaviota/gaviotatb.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QHash>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QTextStream>

using namespace mbc;

namespace training {

namespace {

// One line of the mbChess format: id;fen;first move or -;solution;rating
bool parseLine(const QString &line, Puzzle *p)
{
    const QStringList f = line.split(QLatin1Char(';'));
    if (f.size() < 4)
        return false;
    p->id = f.at(0).trimmed();
    p->fen = f.at(1).trimmed();
    p->firstMove = f.at(2).trimmed() == QLatin1String("-") ? QString() : f.at(2).trimmed();
    p->solution = f.at(3).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    p->rating = f.size() > 4 ? f.at(4).toInt() : 0;
    return !p->id.isEmpty() && !p->fen.isEmpty();
}

// One line of the Lichess CSV: PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,...
bool parseLichess(const QString &line, int n, Puzzle *p)
{
    const QStringList f = line.split(QLatin1Char(','));
    if (f.size() < 8 || f.at(0) == QLatin1String("PuzzleId"))
        return false;
    if (!f.at(7).split(QLatin1Char(' ')).contains(QStringLiteral("mateIn%1").arg(n)))
        return false;
    const QStringList moves = f.at(2).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (moves.size() < 2)
        return false;
    p->id = QStringLiteral("lichess:") + f.at(0);
    p->fen = f.at(1);
    p->firstMove = moves.first();
    p->solution = moves.mid(1);
    p->rating = f.at(3).toInt();
    return true;
}

void readFile(const QString &path, int n, bool lichess, QVector<Puzzle> *out, QSet<QString> *ids)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return;
    QTextStream in(&file);
    int lineNo = 0;
    while (!in.atEnd()) {
        const QString line = in.readLine();
        ++lineNo;
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        Puzzle p;
        if (!(lichess ? parseLichess(line, n, &p) : parseLine(line, &p)))
            continue;
        if (ids->contains(p.id))
            continue;
        Position pos;
        if (!pos.setFromFen(p.fen))
            continue;
        if (!p.firstMove.isEmpty() && parseUci(pos, p.firstMove).isNull())
            continue;
        ids->insert(p.id);
        out->push_back(p);
    }
}

} // namespace

QString trainingFolder()
{
    return datafolders::trainingFolder();
}

const QVector<Puzzle> &matePuzzles(int n)
{
    static QHash<int, QVector<Puzzle>> cache;
    n = qBound(1, n, 3);
    auto it = cache.find(n);
    if (it != cache.end())
        return it.value();
    QVector<Puzzle> list;
    QSet<QString> ids;
    readFile(QStringLiteral(":/training/mate%1.txt").arg(n), n, false, &list, &ids);
    // Own collections: "mate2*.txt" in the mbChess format, Lichess CSV files (any name)
    const QDir dir(trainingFolder());
    for (const QString &name : dir.entryList({ QStringLiteral("mate%1*.txt").arg(n) }, QDir::Files, QDir::Name))
        readFile(dir.filePath(name), n, false, &list, &ids);
    for (const QString &name : dir.entryList({ QStringLiteral("*.csv") }, QDir::Files, QDir::Name))
        readFile(dir.filePath(name), n, true, &list, &ids);
    return cache.insert(n, list).value();
}

} // namespace training

namespace {

QString mateKey(int n, const char *what)
{
    return QStringLiteral("training/mate%1/%2").arg(n).arg(QLatin1String(what));
}

QString endgameKey(int type, const char *what)
{
    return QStringLiteral("training/endgame%1/%2").arg(type).arg(QLatin1String(what));
}

QString colorName(Color c)
{
    return c == White ? QCoreApplication::translate("Trainer", "White")
                      : QCoreApplication::translate("Trainer", "Black");
}

bool lightSquare(int sq) { return ((fileOf(sq) + rankOf(sq)) & 1) != 0; }

// A piece of the strong side the lone king could take.
bool hangingPiece(const Position &pos, Color strong)
{
    for (int sq = 0; sq < 64; ++sq) {
        const Piece pc = pos.pieceAt(sq);
        if (pc == NoPiece || colorOf(pc) != strong || typeOf(pc) == King)
            continue;
        if (pos.isSquareAttacked(sq, ~strong) && !pos.isSquareAttacked(sq, strong))
            return true;
    }
    return false;
}

// Gaviota: plies to mate for the given side, -1 draw/lost, -2 unknown.
int gaviotaPlies(const Position &pos, Color winner)
{
    gaviota::Result r;
    if (!gaviota::isOpen() || !gaviota::probe(pos, &r))
        return -2;
    if ((winner == White && r.kind == gaviota::Result::WhiteMates)
        || (winner == Black && r.kind == gaviota::Result::BlackMates))
        return r.plies;
    return -1;
}

} // namespace

Trainer::Trainer(GameManager *gm, LanguageManager *languages, QObject *parent)
    : QObject(parent), m_gm(gm), m_languages(languages)
{
    m_gm->setTrainerMover([this](const Position &pos) { return defenderMove(pos); });
    connect(m_gm, &GameManager::changed, this, &Trainer::onGameChanged);
    connect(m_gm, &GameManager::gameEnded, this, &Trainer::onGameEnded);
}

bool Trainer::isOnBoard() const
{
    return m_kind != Kind::None && m_gm->gameSerial() == m_serial;
}

bool Trainer::isRunning() const
{
    return isOnBoard() && m_state == State::Running && m_gm->settings().hasEngine() && !m_gm->isGameOver();
}

GameSettings Trainer::baseSettings() const
{
    GameSettings g = GameSettings::load(); // name, threads, hash
    g.timeControl = TimeControl();
    g.useBook = false;
    g.useChessbook = false;
    g.engineOwnBook = false;
    g.ponder = false;
    g.strengthElo = 0;
    g.engineOptions.clear();
    g.engine2Path.clear();
    g.builtinPlayer = GameSettings::NoBuiltin;
    g.colorChoice = m_userColor == White ? GameSettings::PlayWhite : GameSettings::PlayBlack;
    g.mode = m_userColor == White ? GameSettings::HumanWhiteVsEngine : GameSettings::HumanBlackVsEngine;
    return g;
}

// ---------------------------------------------------------------------------
// Find the mate

QString Trainer::mateMenuText(int n)
{
    const int total = training::matePuzzles(n).size();
    const int solved = QSettings().value(mateKey(n, "solved")).toStringList().size();
    //: menu Training > Find the mate: "Mate in 2 (12 of 719 solved)"
    return tr("Mate in %n (%1 of %2 solved)", nullptr, n).arg(qMin(solved, total)).arg(total);
}

void Trainer::resetMateProgress()
{
    QSettings s;
    for (int n = 1; n <= 3; ++n) {
        s.remove(mateKey(n, "solved"));
        s.remove(mateKey(n, "index"));
        s.remove(mateKey(n, "tries"));
    }
}

bool Trainer::prepareMate(int n, GameSettings *settings, Position *start, QString *error)
{
    const QVector<training::Puzzle> &list = training::matePuzzles(n);
    if (list.isEmpty()) {
        *error = tr("There are no puzzles \"mate in %n\".", nullptr, n);
        return false;
    }
    QSettings s;
    const QSet<QString> solved = [&] {
        const QStringList l = s.value(mateKey(n, "solved")).toStringList();
        return QSet<QString>(l.begin(), l.end());
    }();
    // the next one after the last one shown, that is not solved yet (in order of difficulty)
    int index = s.value(mateKey(n, "index"), -1).toInt();
    if (m_kind == Kind::Mate && m_mateMoves == n)
        index = m_puzzleIndex;
    int pick = -1;
    for (int k = 1; k <= list.size(); ++k) {
        const int i = (index + k) % list.size();
        if (!solved.contains(list.at(i).id)) {
            pick = i;
            break;
        }
    }
    if (pick < 0)
        pick = (index + 1) % list.size(); // all solved: once more from the start
    const training::Puzzle &p = list.at(pick);
    Position pos;
    if (!pos.setFromFen(p.fen)) {
        *error = tr("The puzzle %1 has an invalid position.").arg(p.id);
        return false;
    }
    Position task = pos;
    if (!p.firstMove.isEmpty())
        task.makeMove(parseUci(pos, p.firstMove));
    s.setValue(mateKey(n, "index"), pick);

    m_kind = Kind::Mate;
    m_mateMoves = n;
    m_puzzleIndex = pick;
    m_puzzle = p;
    m_start = pos;
    m_taskPosition = task;
    m_offset = p.firstMove.isEmpty() ? 0 : 1;
    m_userColor = task.sideToMove();
    m_solver.clear();

    GameSettings g = baseSettings();
    g.builtinPlayer = GameSettings::BuiltinTrainer;
    g.enginePath.clear();
    g.opponentName = tr("Puzzle");
    *settings = g;
    *start = pos;
    return true;
}

bool Trainer::prepareAgain(GameSettings *settings, Position *start, QString *error)
{
    if (m_kind == Kind::None) {
        *error = tr("There is no task.");
        return false;
    }
    GameSettings g = baseSettings();
    if (m_kind == Kind::Mate) {
        g.builtinPlayer = GameSettings::BuiltinTrainer;
        g.opponentName = tr("Puzzle");
    } else {
        QString name;
        g.enginePath = enginePath(&name);
        if (g.enginePath.isEmpty()) {
            *error = tr("No engine was found for the defence.");
            return false;
        }
        g.moveTimeMs = 1000;
    }
    m_solver.clear();
    *settings = g;
    *start = m_start;
    return true;
}

void Trainer::started()
{
    m_serial = m_gm->gameSerial();
    m_state = State::Running;
    m_mistakes = 0;
    m_helped = false;
    m_lastPly = 0;
    if (m_kind == Kind::Mate) {
        m_message = m_offset ? tr("Watch the opponent's move, then find the mate.") : tr("Find the mate.");
        QSettings s;
        s.setValue(mateKey(m_mateMoves, "tries"), s.value(mateKey(m_mateMoves, "tries"), 0).toInt() + 1);
    } else {
        if (m_endgame == PawnWin || m_endgame == PawnDraw)
            m_message = tr("mbChess knows every king and pawn position and checks each of your moves.");
        else
            m_message = m_tablebase ? tr("The Gaviota tablebases check each of your moves.")
                                    : tr("Without Gaviota tablebases only the move limit and the result are "
                                         "checked.");
        QSettings s;
        s.setValue(endgameKey(m_endgame, "tries"), s.value(endgameKey(m_endgame, "tries"), 0).toInt() + 1);
    }
    emit changed();
}

void Trainer::stop()
{
    m_kind = Kind::None;
    m_serial = -1;
    emit changed();
}

int Trainer::userMovesDone() const
{
    const int plies = m_gm->game().nodePly(m_gm->mainLineEnd()) - m_offset;
    return plies <= 0 ? 0 : (plies + 1) / 2;
}

Move Trainer::defenderMove(const Position &pos)
{
    if (m_kind != Kind::Mate || !isOnBoard())
        return Move();
    if (m_offset && pos == m_start)
        return parseUci(pos, m_puzzle.firstMove);
    const int left = m_mateMoves - userMovesDone();
    Move m = m_solver.bestDefence(pos, qMax(1, left));
    if (m.isNull()) {
        const MoveList moves = pos.legalMoves();
        if (!moves.empty())
            m = moves.front();
    }
    return m;
}

bool Trainer::userMove(Move move)
{
    if (!isRunning() || m_kind != Kind::Mate)
        return true;
    const Position &pos = m_gm->game().nodePosition(m_gm->mainLineEnd());
    if (pos.sideToMove() != m_userColor || !pos.isLegal(move))
        return true;
    const int left = m_mateMoves - userMovesDone();
    const Position next = pos.after(move);
    if (next.isCheckmate() || (left > 1 && m_solver.isMatedWithin(next, left - 1))) {
        if (!next.isCheckmate())
            m_message = tr("Correct! Go on.");
        emit changed();
        return true;
    }
    ++m_mistakes;
    m_message = tr("%1 does not mate in time. Try again.").arg(san(pos, move));
    emit changed();
    return false;
}

QString Trainer::san(const Position &pos, Move m) const
{
    return localizeSan(toSan(pos, m), m_languages->pieceLetters());
}

QString Trainer::sanLine(const Position &pos, const QStringList &uci) const
{
    Position p = pos;
    QStringList out;
    for (const QString &u : uci) {
        const Move m = parseUci(p, u);
        if (m.isNull() || !p.isLegal(m))
            break;
        QString s = san(p, m);
        if (p.sideToMove() == White)
            s = QStringLiteral("%1. %2").arg(p.fullmoveNumber()).arg(s);
        else if (out.isEmpty())
            s = QStringLiteral("%1... %2").arg(p.fullmoveNumber()).arg(s);
        out << s;
        p.makeMove(m);
    }
    return out.join(QLatin1Char(' '));
}

QStringList Trainer::mainLine(const Position &pos, int moves)
{
    QStringList line;
    Position q = pos;
    for (int left = moves; left > 0;) {
        int l = 0;
        const Move a = m_solver.bestAttack(q, left, &l);
        if (a.isNull())
            break;
        line << toUci(a);
        q.makeMove(a);
        if (q.isCheckmate() || l <= 1)
            break;
        const Move r = m_solver.bestDefence(q, l - 1, &left);
        if (r.isNull())
            break;
        line << toUci(r);
        q.makeMove(r);
    }
    return line;
}

QString Trainer::hint()
{
    if (!isOnBoard())
        return QString();
    const Position &pos = m_gm->game().nodePosition(m_gm->mainLineEnd());
    if (m_kind == Kind::Mate) {
        if (!isRunning() || pos.sideToMove() != m_userColor)
            return QString();
        const int left = m_mateMoves - userMovesDone();
        const MoveList keys = m_solver.keyMoves(pos, qMax(1, left));
        if (keys.empty())
            return tr("There is no mate any more: take the last move back.");
        static const char *const pieces[] = {
            QT_TRANSLATE_NOOP("Trainer", "a pawn"), QT_TRANSLATE_NOOP("Trainer", "the knight"),
            QT_TRANSLATE_NOOP("Trainer", "the bishop"), QT_TRANSLATE_NOOP("Trainer", "the rook"),
            QT_TRANSLATE_NOOP("Trainer", "the queen"), QT_TRANSLATE_NOOP("Trainer", "the king"),
        };
        const Move k = keys.front();
        const PieceType t = typeOf(pos.pieceAt(k.from()));
        const Position next = pos.after(k);
        QString text = tr("Move %1.").arg(tr(pieces[t]));
        if (next.inCheck())
            text += QLatin1Char(' ') + tr("The first move gives check.");
        else if (pos.isCapture(k))
            text += QLatin1Char(' ') + tr("The first move captures.");
        else
            text += QLatin1Char(' ') + tr("The first move is a quiet move.");
        return text;
    }
    // Endgames: the tablebases know the best moves
    if (!isRunning() || pos.sideToMove() != m_userColor)
        return QString();
    QStringList good;
    int best = 1000;
    for (const Move m : pos.legalMoves()) {
        const Position next = pos.after(m);
        if (m_endgame == PawnDraw) {
            const kpk::Result r = kpk::probe(next);
            if (r == kpk::Result::Draw || popCount(next.occupancy()) < 3)
                good << san(pos, m);
            continue;
        }
        if (next.isCheckmate()) {
            good = QStringList{ san(pos, m) };
            best = 0;
            break;
        }
        int plies = gaviotaPlies(next, m_userColor);
        if (plies == -2 && m_endgame == PawnWin) {
            if (m.isPromotion())
                plies = m.promotion() == Queen && !next.isStalemate() && !hangingPiece(next, m_userColor) ? 0 : -1;
            else
                plies = kpk::probe(next) == kpk::Result::Win ? 500 : -1;
        }
        if (plies < 0)
            continue;
        if (plies < best) {
            best = plies;
            good = QStringList{ san(pos, m) };
        } else if (plies == best) {
            good << san(pos, m);
        }
    }
    if (m_endgame == PawnWin || m_endgame == PawnDraw || gaviota::isOpen()) {
        if (good.isEmpty())
            return tr("No move saves it any more.");
        return tr("Good moves: %1").arg(good.mid(0, 3).join(QStringLiteral(", ")));
    }
    switch (m_endgame) {
    case QueenMate:
        return tr("Restrict the king with the queen (a knight's move away from it), drive it to the edge, "
                  "then bring your king up. Beware of stalemate!");
    case RookMate:
        return tr("Use your king and rook together: the rook cuts off the king, your king takes the opposition, "
                  "then the rook checks it back towards the edge.");
    case TwoBishops:
        return tr("The bishops side by side build a wall the king cannot cross. Drive the king to a corner, "
                  "with your king close by.");
    default:
        return tr("Drive the king to a corner of the bishop's colour; the knight and the king help the bishop "
                  "(\"W manoeuvre\" of the knight).");
    }
}

QString Trainer::solution()
{
    if (m_kind != Kind::Mate || !isOnBoard())
        return QString();
    m_helped = true;
    const Position &pos = m_gm->game().nodePosition(m_gm->mainLineEnd());
    if (pos.sideToMove() != m_userColor) // from the start of the task
        return sanLine(m_taskPosition, m_puzzle.solution);
    const int left = m_mateMoves - userMovesDone();
    const QStringList line = mainLine(pos, qMax(1, left));
    if (line.isEmpty())
        return tr("There is no mate any more: take the last move back.");
    m_message = tr("The puzzle comes again later.");
    emit changed();
    return sanLine(pos, line);
}

void Trainer::finish(bool solved, const QString &message)
{
    if (m_state != State::Running)
        return;
    m_state = solved ? State::Solved : State::Failed;
    m_message = message;
    QSettings s;
    if (m_kind == Kind::Mate && solved && !m_helped && m_mistakes == 0) {
        QStringList list = s.value(mateKey(m_mateMoves, "solved")).toStringList();
        if (!list.contains(m_puzzle.id))
            list << m_puzzle.id;
        s.setValue(mateKey(m_mateMoves, "solved"), list);
    }
    if (m_kind == Kind::Endgame && solved)
        s.setValue(endgameKey(m_endgame, "solved"), s.value(endgameKey(m_endgame, "solved"), 0).toInt() + 1);
    emit changed();
    emit finished(solved);
}

void Trainer::onGameEnded(Result result, const QString &reason)
{
    if (!isOnBoard() || m_state != State::Running)
        return;
    const bool userWon = (result == Result::WhiteWins && m_userColor == White)
        || (result == Result::BlackWins && m_userColor == Black);
    if (m_kind == Kind::Mate) {
        if (userWon)
            finish(true, m_mistakes || m_helped ? tr("Mate! Solved, but with help or mistakes: it comes again later.")
                                                : tr("Mate! Puzzle solved."));
        else
            finish(false, reason);
        return;
    }
    if (m_endgame == PawnDraw) {
        if (result == Result::Draw)
            finish(true, tr("Draw (%1). Well defended!").arg(reason));
        else
            finish(false, reason);
        return;
    }
    if (userWon) {
        const int moves = userMovesDone();
        if (moves <= m_limit)
            finish(true, tr("Mate in %n move(s). Well done!", nullptr, moves));
        else
            finish(false, tr("Mate, but only after %n moves: the limit was exceeded.", nullptr, moves));
    } else {
        finish(false, reason);
    }
}

void Trainer::onGameChanged()
{
    if (!isRunning())
        return;
    const Game &game = m_gm->game();
    const Game::NodeId end = m_gm->mainLineEnd();
    const int ply = game.nodePly(end);
    if (ply < m_lastPly) { // taken back
        m_lastPly = ply;
        emit changed();
        return;
    }
    if (ply == m_lastPly || end == Game::RootNode)
        return;
    m_lastPly = ply;
    const Position &after = game.nodePosition(end);
    const Position &before = game.nodePosition(game.parent(end));
    if (before.sideToMove() == m_userColor && m_kind == Kind::Endgame)
        checkEndgameMove(before, game.nodeMove(end), after);
    if (m_kind == Kind::Mate && after.sideToMove() == m_userColor) {
        if (ply > m_offset)
            m_message = tr("%n move(s) left to mate.", nullptr, m_mateMoves - userMovesDone());
        else
            m_message = tr("Your move: find the mate.");
    }
    emit changed();
}

void Trainer::checkEndgameMove(const Position &before, Move move, const Position &after)
{
    Q_UNUSED(before)
    if (after.isCheckmate() || after.status() != GameStatus::Ongoing)
        return; // onGameEnded() decides
    const int done = userMovesDone();
    if (m_endgame == PawnDraw) {
        if (kpk::probe(after) == kpk::Result::Win) {
            finish(false, tr("That loses: now %1 wins by force.").arg(colorName(~m_userColor)));
            m_gm->stopPlaying();
            return;
        }
        if (done >= m_limit) {
            finish(true, tr("Held for %n moves: a draw. Well defended!", nullptr, done));
            m_gm->stopPlaying();
        }
        return;
    }
    if (m_endgame == PawnWin) {
        if (move.isPromotion()) {
            const int plies = gaviotaPlies(after, m_userColor);
            bool ok;
            if (plies != -2)
                ok = plies >= 0;
            else
                ok = !after.isStalemate() && !hangingPiece(after, m_userColor);
            if (ok)
                finish(true, tr("Promoted in %n move(s): the win is yours.", nullptr, done));
            else
                finish(false, tr("Promoted, but the new piece is lost or it is stalemate."));
            m_gm->stopPlaying();
            return;
        }
        if (kpk::probe(after) != kpk::Result::Win) {
            finish(false, tr("That throws the win away: the position is a draw now."));
            m_gm->stopPlaying();
            return;
        }
        if (done >= m_limit) {
            finish(false, tr("The pawn was not promoted within %n moves.", nullptr, m_limit));
            m_gm->stopPlaying();
        }
        return;
    }
    // basic mates
    const int plies = gaviotaPlies(after, m_userColor);
    if (plies == -1) {
        finish(false, hangingPiece(before, m_userColor) || popCount(after.occupancy()) < popCount(before.occupancy())
                          ? tr("A piece is lost: the win is gone.")
                          : tr("That throws the win away: the position is a draw now."));
        m_gm->stopPlaying();
        return;
    }
    if (done >= m_limit) {
        finish(false, tr("No mate within %n moves.", nullptr, m_limit));
        m_gm->stopPlaying();
        return;
    }
    if (plies >= 0) {
        const int mateIn = (plies + 1) / 2; // the user's moves still needed
        const int left = m_limit - done;
        m_message = mateIn > left ? tr("Mate in %1 is still possible, but only %n move(s) are left: hurry!", nullptr,
                                       left).arg(mateIn)
                                  : tr("Mate in %1 with best play; %n move(s) left.", nullptr, left).arg(mateIn);
    } else {
        m_message = tr("%n move(s) left.", nullptr, m_limit - done);
    }
}

// ---------------------------------------------------------------------------
// Endgames

QString Trainer::endgameName(int type)
{
    switch (type) {
    case QueenMate:
        return tr("King and queen against king (KQK)");
    case RookMate:
        return tr("King and rook against king (KRK)");
    case TwoBishops:
        return tr("Two bishops (KBBK)");
    case BishopKnight:
        return tr("Bishop and knight (KBNK)");
    case PawnWin:
        return tr("King and pawn: win");
    case PawnDraw:
        return tr("King against king and pawn: hold the draw");
    }
    return QString();
}

QString Trainer::enginePath(QString *name) const
{
    // Texel reads the Gaviota tables itself
    opponents::Opponent o;
    o.engines = QStringList{ QStringLiteral("Texel"), QStringLiteral("Stockfish"), QStringLiteral("Arasan"),
                             QStringLiteral("Patricia"), QStringLiteral("Fruit"), QStringLiteral("Toga II") };
    QString path = opponents::enginePath(o, name);
    if (path.isEmpty()) {
        const QVector<EngineEntry> engines = enginemanager::engines();
        if (!engines.isEmpty()) {
            path = engines.first().path;
            if (name)
                *name = engines.first().name;
        }
    }
    return path;
}

bool Trainer::generate(int type, Position *out) const
{
    auto *rng = QRandomGenerator::global();
    QVector<PieceType> pieces;
    switch (type) {
    case QueenMate: pieces = { Queen }; break;
    case RookMate: pieces = { Rook }; break;
    case TwoBishops: pieces = { Bishop, Bishop }; break;
    case BishopKnight: pieces = { Bishop, Knight }; break;
    default: pieces = { Pawn }; break;
    }
    const bool pawn = type == PawnWin || type == PawnDraw;
    // the side to move: the user (White, or Black defending against the pawn)
    const Color toMove = type == PawnDraw ? Black : White;
    // Gaviota: not too easy, and room for mistakes within the move limit
    static const int minMoves[] = { 6, 10, 10, 18, 0, 0 };
    for (int attempt = 0; attempt < 20000; ++attempt) {
        Position p;
        const int wk = int(rng->bounded(64));
        const int bk = int(rng->bounded(64));
        if (wk == bk || (qAbs(fileOf(wk) - fileOf(bk)) <= 1 && qAbs(rankOf(wk) - rankOf(bk)) <= 1))
            continue;
        p.setPiece(wk, makePiece(White, King));
        p.setPiece(bk, makePiece(Black, King));
        bool ok = true;
        int firstBishop = -1;
        for (PieceType t : pieces) {
            int sq = int(rng->bounded(64));
            if (t == Pawn)
                sq = 8 + int(rng->bounded(32)); // 2nd to 5th rank
            if (p.pieceAt(sq) != NoPiece) {
                ok = false;
                break;
            }
            if (t == Bishop && type == TwoBishops) {
                if (firstBishop >= 0 && lightSquare(firstBishop) == lightSquare(sq)) {
                    ok = false;
                    break;
                }
                firstBishop = sq;
            }
            p.setPiece(sq, makePiece(White, t));
        }
        if (!ok)
            continue;
        p.setSideToMove(toMove);
        p.setCastlingRights(0);
        p.setEnPassantSquare(NoSquare);
        p.setMoveCounters(0, 1);
        if (!p.isValid() || p.inCheck() || p.legalMoves().empty() || hangingPiece(p, White))
            continue;
        if (pawn) {
            const kpk::Result r = kpk::probe(p);
            const int psq = lsb(p.pieces(White, Pawn));
            const int promo = 56 + fileOf(psq);
            if (type == PawnWin) {
                if (r != kpk::Result::Win)
                    continue;
                // not a pawn that simply runs: the black king is inside its square
                const int pawnSteps = 7 - rankOf(psq) - (rankOf(psq) == 1 ? 1 : 0);
                const int kingSteps = qMax(qAbs(fileOf(bk) - fileOf(promo)), qAbs(rankOf(bk) - rankOf(promo)));
                if (kingSteps > pawnSteps)
                    continue;
                if (fileOf(psq) == 0 || fileOf(psq) == 7) // rook pawns are mostly draws
                    continue;
            } else {
                if (r != kpk::Result::Draw)
                    continue;
                // a draw, but one wrong king move loses
                bool canLose = false;
                for (const Move m : p.legalMoves()) {
                    const Position n = p.after(m);
                    if (popCount(n.occupancy()) == 3 && kpk::probe(n) == kpk::Result::Win)
                        canLose = true;
                }
                if (!canLose)
                    continue;
                // the black king may not be able to take the pawn at once
                if (p.isSquareAttacked(psq, Black))
                    continue;
            }
            *out = p;
            return true;
        }
        // basic mates: the lone king away from the edge, a real task
        if (rankOf(bk) == 0 || rankOf(bk) == 7 || fileOf(bk) == 0 || fileOf(bk) == 7)
            continue;
        const int plies = gaviotaPlies(p, White);
        if (plies == -1)
            continue;
        if (plies >= 0) {
            const int moves = (plies + 1) / 2;
            if (moves < minMoves[type] && attempt < 15000)
                continue;
        }
        *out = p;
        return true;
    }
    return false;
}

bool Trainer::prepareEndgame(int type, GameSettings *settings, Position *start, QString *error)
{
    if (type < 0 || type >= EndgameCount)
        return false;
    QString engineName;
    const QString path = enginePath(&engineName);
    if (path.isEmpty()) {
        *error = tr("No engine was found for the defence. Engines > Manage engines adds one.");
        return false;
    }
    Position pos;
    if (!generate(type, &pos)) {
        *error = tr("No position was found for this endgame.");
        return false;
    }
    static const int limits[] = { 15, 25, 30, 40, 20, 20 };
    m_kind = Kind::Endgame;
    m_endgame = type;
    m_limit = limits[type];
    m_start = pos;
    m_taskPosition = pos;
    m_offset = 0;
    m_userColor = pos.sideToMove();
    const int plies = gaviotaPlies(pos, White);
    m_tablebase = plies >= 0 || type == PawnWin || type == PawnDraw;
    m_startDtm = plies >= 0 ? (plies + 1) / 2 : 0;

    GameSettings g = baseSettings();
    g.enginePath = path;
    g.moveTimeMs = 1000;
    *settings = g;
    *start = pos;
    return true;
}

// ---------------------------------------------------------------------------
// Texts

QString Trainer::title() const
{
    if (m_kind == Kind::Mate) {
        //: panel Training: "Mate in 2 - puzzle 15 of 719"
        return tr("Mate in %n - puzzle %1 of %2", nullptr, m_mateMoves)
            .arg(m_puzzleIndex + 1)
            .arg(training::matePuzzles(m_mateMoves).size());
    }
    if (m_kind == Kind::Endgame)
        return endgameName(m_endgame);
    return tr("Training");
}

QString Trainer::task() const
{
    if (m_kind == Kind::Mate)
        return tr("%1 to move mates in %n move(s).", nullptr, m_mateMoves).arg(colorName(m_userColor));
    if (m_kind != Kind::Endgame)
        return tr("Choose a task in the menu Training.");
    switch (m_endgame) {
    case PawnWin:
        return tr("White to move: promote the pawn within %n moves, without throwing the win away.", nullptr, m_limit);
    case PawnDraw:
        return tr("Black to move: hold the draw for %n moves, or take the pawn.", nullptr, m_limit);
    default:
        break;
    }
    QString text = tr("White to move: mate within %n moves.", nullptr, m_limit);
    if (m_startDtm > 0)
        text += QLatin1Char(' ') + tr("With best play it is mate in %n.", nullptr, m_startDtm);
    return text;
}

QString Trainer::source() const
{
    if (m_kind != Kind::Mate)
        return QString();
    const QString id = m_puzzle.id;
    QString text;
    if (id.startsWith(QLatin1String("lichess:"))) {
        const QString code = id.mid(8);
        text = tr("Lichess puzzle <a href=\"https://lichess.org/training/%1\">%1</a> (CC0)").arg(code);
    } else if (id.startsWith(QLatin1String("mbchess:"))) {
        text = tr("mbChess puzzle \"%1\"").arg(id.mid(8));
    } else {
        text = tr("Puzzle %1").arg(id.toHtmlEscaped());
    }
    if (m_puzzle.rating > 0)
        text += QStringLiteral(" · ") + tr("rating %1").arg(m_puzzle.rating);
    return text;
}

QString Trainer::progress() const
{
    QSettings s;
    if (m_kind == Kind::Mate) {
        const int solved = s.value(mateKey(m_mateMoves, "solved")).toStringList().size();
        QString text = tr("Solved: %1 of %2").arg(solved).arg(training::matePuzzles(m_mateMoves).size());
        if (m_mistakes > 0)
            text += QStringLiteral(" · ") + tr("mistakes in this puzzle: %1").arg(m_mistakes);
        return text;
    }
    if (m_kind == Kind::Endgame)
        return tr("Won: %1 of %2 tries")
            .arg(s.value(endgameKey(m_endgame, "solved"), 0).toInt())
            .arg(s.value(endgameKey(m_endgame, "tries"), 0).toInt());
    return QString();
}
