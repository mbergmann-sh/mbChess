// SPDX-License-Identifier: GPL-3.0-or-later
#include "trainer.h"

#include "datafolders.h"
#include "enginemanager.h"
#include "gamemanager.h"
#include "languagemanager.h"
#include "opponents.h"

#include <book/polyglot.h>
#include <chesscore/kpk.h>
#include <chesscore/notation.h>
#include <gaviota/gaviotatb.h>
#include <pgn/pgn.h>

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <algorithm>
#include <QHash>
#include <QLocale>
#include <QRandomGenerator>
#include <QSet>
#include <QSettings>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

using namespace mbc;

namespace training {

namespace {

// One line of the mbChess format: id;fen;first move or -;solution;rating[;themes]
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
    if (f.size() > 5)
        p->themes = f.at(5).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    return !p->id.isEmpty() && !p->fen.isEmpty();
}

// One line of the Lichess CSV: PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,...
// n > 0: the mates in n moves; n = 0: tactics (no mates).
bool parseLichess(const QString &line, int n, Puzzle *p)
{
    const QStringList f = line.split(QLatin1Char(','));
    if (f.size() < 8 || f.at(0) == QLatin1String("PuzzleId"))
        return false;
    const QStringList themes = f.at(7).split(QLatin1Char(' '), Qt::SkipEmptyParts);
    if (n > 0 ? !themes.contains(QStringLiteral("mateIn%1").arg(n)) : themes.contains(QStringLiteral("mate")))
        return false;
    p->themes = themes;
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

QString repertoireFolder()
{
    return trainingFolder() + QStringLiteral("/Repertoire");
}

QVector<Repertoire> repertoires()
{
    QStringList files;
    const QDir builtIn(QStringLiteral(":/training/repertoire"));
    for (const QString &name : builtIn.entryList({ QStringLiteral("*.pgn") }, QDir::Files, QDir::Name))
        files << builtIn.filePath(name);
    const QDir own(repertoireFolder());
    for (const QString &name : own.entryList({ QStringLiteral("*.pgn"), QStringLiteral("*.PGN") }, QDir::Files, QDir::Name))
        files << own.filePath(name);
    QVector<Repertoire> out;
    for (const QString &path : files) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly))
            continue;
        // only the tags of the first game
        const QString head = mbc::pgn::decode(f.read(4096));
        Repertoire r;
        r.path = path;
        r.name = QFileInfo(path).completeBaseName();
        const QRegularExpression tag(QStringLiteral("\\[(\\w+)\\s+\"([^\"]*)\"\\]"));
        // [Name_de "..."], [Name_en "..."]: the name in the language of the program
        const QString nameTag = QStringLiteral("Name_") + LanguageManager::savedLanguage();
        QString localName;
        auto it = tag.globalMatch(head);
        while (it.hasNext()) {
            const QRegularExpressionMatch m = it.next();
            if (m.captured(1) == nameTag)
                localName = m.captured(2);
            if (m.captured(1) == QLatin1String("Event") && !m.captured(2).isEmpty() && m.captured(2) != QLatin1String("?"))
                r.name = m.captured(2);
            if (m.captured(1) == QLatin1String("Repertoire"))
                r.color = m.captured(2).compare(QLatin1String("White"), Qt::CaseInsensitive) == 0 ? 0
                    : m.captured(2).compare(QLatin1String("Black"), Qt::CaseInsensitive) == 0      ? 1
                                                                                                    : -1;
        }
        if (!localName.isEmpty())
            r.name = localName;
        out << r;
    }
    return out;
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

const QVector<Puzzle> &tacticPuzzles()
{
    static QVector<Puzzle> list;
    static bool loaded = false;
    if (loaded)
        return list;
    loaded = true;
    QSet<QString> ids;
    readFile(QStringLiteral(":/training/tactics.txt"), 0, false, &list, &ids);
    const QDir dir(trainingFolder());
    for (const QString &name : dir.entryList({ QStringLiteral("tactics*.txt") }, QDir::Files, QDir::Name))
        readFile(dir.filePath(name), 0, false, &list, &ids);
    for (const QString &name : dir.entryList({ QStringLiteral("*.csv") }, QDir::Files, QDir::Name))
        readFile(dir.filePath(name), 0, true, &list, &ids);
    // in order of difficulty, own files included
    std::stable_sort(list.begin(), list.end(), [](const Puzzle &a, const Puzzle &b) { return a.rating < b.rating; });
    return list;
}

QVector<int> tacticIndexes(const QString &theme)
{
    static QHash<QString, QVector<int>> cache;
    auto it = cache.find(theme);
    if (it != cache.end())
        return it.value();
    QVector<int> out;
    const QVector<Puzzle> &list = tacticPuzzles();
    for (int i = 0; i < list.size(); ++i)
        if (theme.isEmpty() || list.at(i).themes.contains(theme))
            out << i;
    return cache.insert(theme, out).value();
}

const QVector<TacticTheme> &tacticThemes()
{
    // the same list as tools/make-tactics-puzzles.py
    static const QVector<TacticTheme> themes = {
        { "hangingPiece", QT_TRANSLATE_NOOP("Trainer", "Hanging piece"), 0 },
        { "fork", QT_TRANSLATE_NOOP("Trainer", "Fork"), 0 },
        { "pin", QT_TRANSLATE_NOOP("Trainer", "Pin"), 0 },
        { "skewer", QT_TRANSLATE_NOOP("Trainer", "Skewer"), 0 },
        { "discoveredAttack", QT_TRANSLATE_NOOP("Trainer", "Discovered attack"), 0 },
        { "doubleCheck", QT_TRANSLATE_NOOP("Trainer", "Double check"), 0 },
        { "trappedPiece", QT_TRANSLATE_NOOP("Trainer", "Trapped piece"), 0 },
        { "deflection", QT_TRANSLATE_NOOP("Trainer", "Deflection"), 1 },
        { "attraction", QT_TRANSLATE_NOOP("Trainer", "Attraction"), 1 },
        { "sacrifice", QT_TRANSLATE_NOOP("Trainer", "Sacrifice"), 1 },
        { "capturingDefender", QT_TRANSLATE_NOOP("Trainer", "Capture the defender"), 1 },
        { "clearance", QT_TRANSLATE_NOOP("Trainer", "Clearance"), 1 },
        { "interference", QT_TRANSLATE_NOOP("Trainer", "Interference"), 1 },
        { "intermezzo", QT_TRANSLATE_NOOP("Trainer", "Intermediate move"), 1 },
        { "xRayAttack", QT_TRANSLATE_NOOP("Trainer", "X-ray attack"), 1 },
        { "quietMove", QT_TRANSLATE_NOOP("Trainer", "Quiet move"), 2 },
        { "defensiveMove", QT_TRANSLATE_NOOP("Trainer", "Defensive move"), 2 },
        { "zugzwang", QT_TRANSLATE_NOOP("Trainer", "Zugzwang"), 2 },
        { "promotion", QT_TRANSLATE_NOOP("Trainer", "Promotion"), 2 },
        { "kingsideAttack", QT_TRANSLATE_NOOP("Trainer", "Attack on the king"), 3 },
        { "pawnEndgame", QT_TRANSLATE_NOOP("Trainer", "Pawn endgame"), 3 },
        { "rookEndgame", QT_TRANSLATE_NOOP("Trainer", "Rook endgame"), 3 },
    };
    return themes;
}

QString tacticThemeName(const QString &id)
{
    if (id.isEmpty())
        return QCoreApplication::translate("Trainer", "Mixed");
    for (const TacticTheme &t : tacticThemes())
        if (id == QLatin1String(t.id))
            return QCoreApplication::translate("Trainer", t.name);
    return id;
}

QString tacticGroupName(int group)
{
    switch (group) {
    case 0: return QCoreApplication::translate("Trainer", "Basic motifs");
    case 1: return QCoreApplication::translate("Trainer", "Combinations");
    case 2: return QCoreApplication::translate("Trainer", "Special moves");
    default: return QCoreApplication::translate("Trainer", "Attack and endgame");
    }
}

} // namespace training

namespace {

QString tacticsKey(const char *what)
{
    return QStringLiteral("training/tactics/%1").arg(QLatin1String(what));
}

QString tacticsIndexKey(const QString &theme)
{
    return QStringLiteral("training/tactics/index/%1").arg(theme.isEmpty() ? QStringLiteral("mixed") : theme);
}

QSet<QString> settingsSet(const QString &key)
{
    const QStringList l = QSettings().value(key).toStringList();
    return QSet<QString>(l.begin(), l.end());
}

QString mateKey(int n, const char *what)
{
    return QStringLiteral("training/mate%1/%2").arg(n).arg(QLatin1String(what));
}

// "training/opening/<file name or book>/<colour>/<what>"
QString openingKey(const QString &source, Color color, const char *what)
{
    const QString name = source == QLatin1String("book") ? source : QFileInfo(source).fileName();
    return QStringLiteral("training/opening/%1/%2/%3")
        .arg(name, color == White ? QStringLiteral("white") : QStringLiteral("black"), QLatin1String(what));
}

// a position without the move counters, as key of the repertoire tree
QString positionKey(const Position &pos)
{
    return pos.toFen().section(QLatin1Char(' '), 0, 3);
}

void addTree(const Game &game, Game::NodeId id, QHash<QString, QVector<QPair<QString, int>>> *tree)
{
    const Position &pos = game.nodePosition(id);
    for (Game::NodeId child : game.children(id)) {
        const QString uci = toUci(game.nodeMove(child));
        QVector<QPair<QString, int>> &moves = (*tree)[positionKey(pos)];
        bool found = false;
        for (auto &m : moves)
            if (m.first == uci) {
                ++m.second;
                found = true;
            }
        if (!found)
            moves.append({ uci, 1 });
        addTree(game, child, tree);
    }
}

QString resistanceKey(Color color, int limit, const char *what)
{
    return QStringLiteral("training/resistance/%1/%2/%3")
        .arg(color == White ? QStringLiteral("white") : QStringLiteral("black"))
        .arg(limit)
        .arg(QLatin1String(what));
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

QString Trainer::tacticsMenuText(const QString &theme)
{
    const QVector<int> idx = training::tacticIndexes(theme);
    const QSet<QString> solved = settingsSet(tacticsKey("solved"));
    int done = 0;
    for (int i : idx)
        if (solved.contains(training::tacticPuzzles().at(i).id))
            ++done;
    //: menu Training > Tactics: "Fork (3 of 378 solved)"
    return tr("%1 (%2 of %3 solved)").arg(training::tacticThemeName(theme)).arg(done).arg(idx.size());
}

void Trainer::resetTacticsProgress()
{
    QSettings().remove(QStringLiteral("training/tactics"));
}

bool Trainer::prepareTactics(const QString &theme, GameSettings *settings, Position *start, QString *error)
{
    const QVector<training::Puzzle> &all = training::tacticPuzzles();
    const QVector<int> idx = training::tacticIndexes(theme);
    if (idx.isEmpty()) {
        *error = tr("There are no puzzles for \"%1\".").arg(training::tacticThemeName(theme));
        return false;
    }
    QSettings s;
    const QSet<QString> solved = settingsSet(tacticsKey("solved"));
    const QStringList retry = s.value(tacticsKey("retry")).toStringList();
    int index = s.value(tacticsIndexKey(theme), -1).toInt();
    if (m_kind == Kind::Tactics && m_theme == theme)
        index = m_puzzleIndex;
    int pick = -1;
    // a puzzle that went wrong comes again after three others
    if (m_sinceRetry >= 3) {
        for (const QString &id : retry) {
            for (int k = 0; k < idx.size(); ++k)
                if (all.at(idx.at(k)).id == id) {
                    pick = k;
                    break;
                }
            if (pick >= 0)
                break;
        }
    }
    if (pick >= 0) {
        m_sinceRetry = 0;
    } else {
        ++m_sinceRetry;
        for (int k = 1; k <= idx.size(); ++k) {
            const int i = (index + k) % idx.size();
            if (!solved.contains(all.at(idx.at(i)).id) && !retry.contains(all.at(idx.at(i)).id)) {
                pick = i;
                break;
            }
        }
        if (pick < 0)
            pick = (index + 1) % idx.size();
        s.setValue(tacticsIndexKey(theme), pick);
    }
    const training::Puzzle &p = all.at(idx.at(pick));
    Position pos;
    if (!pos.setFromFen(p.fen)) {
        *error = tr("The puzzle %1 has an invalid position.").arg(p.id);
        return false;
    }
    Position task = pos;
    if (!p.firstMove.isEmpty())
        task.makeMove(parseUci(pos, p.firstMove));

    m_kind = Kind::Tactics;
    m_theme = theme;
    m_puzzleIndex = pick;
    m_puzzle = p;
    m_start = pos;
    m_taskPosition = task;
    m_offset = p.firstMove.isEmpty() ? 0 : 1;
    m_userColor = task.sideToMove();

    GameSettings g = baseSettings();
    g.builtinPlayer = GameSettings::BuiltinTrainer;
    g.enginePath.clear();
    g.opponentName = tr("Puzzle");
    *settings = g;
    *start = pos;
    return true;
}

// ---------------------------------------------------------------------------
// Openings

QString Trainer::openingMenuText(const QString &name, const QString &source, Color color)
{
    QSettings s;
    const int done = s.value(openingKey(source, color, "perfect"), 0).toInt();
    const int tries = s.value(openingKey(source, color, "tries"), 0).toInt();
    if (tries == 0)
        return name;
    //: menu Training > Openings: "Italian Game (5 of 7 lines without a mistake)"
    return tr("%1 (%2 of %3 lines without a mistake)").arg(name).arg(done).arg(tries);
}

bool Trainer::prepareOpening(const QString &source, Color color, GameSettings *settings, Position *start,
                             QString *error)
{
    QHash<QString, QVector<QPair<QString, int>>> tree;
    QString name;
    if (source == QLatin1String("book")) {
        const GameSettings g = GameSettings::load();
        const QString file = g.bookFile.isEmpty() ? QStringLiteral(":/books/mbchess.bin") : g.bookFile;
        QFile f(file);
        mbc::book::PolyglotBook book;
        QString err;
        if (!f.open(QIODevice::ReadOnly) || !book.setData(f.readAll(), &err)) {
            *error = tr("The opening book %1 cannot be read. %2").arg(QDir::toNativeSeparators(file), err);
            return false;
        }
        m_bookDepth = qBound(4, g.bookDepth, 20);
        // the book as a tree: all positions reachable with book moves up to the depth
        QVector<Position> todo { Position::startPosition() };
        QSet<QString> seen;
        while (!todo.isEmpty()) {
            const Position pos = todo.takeLast();
            const QString key = positionKey(pos);
            if (seen.contains(key) || pos.fullmoveNumber() > m_bookDepth)
                continue;
            seen.insert(key);
            QVector<QPair<QString, int>> moves;
            for (const mbc::book::BookMove &b : book.moves(pos)) {
                if (b.weight <= 0 || !pos.isLegal(b.move))
                    continue;
                moves.append({ toUci(b.move), b.weight });
                todo.append(pos.after(b.move));
            }
            if (!moves.isEmpty())
                tree.insert(key, moves);
        }
        name = g.bookFile.isEmpty() ? tr("mbChess opening book") : QFileInfo(file).completeBaseName();
    } else {
        QVector<Game> games;
        QString err;
        if (!mbc::pgn::readFile(source, &games, &err) || games.isEmpty()) {
            *error = tr("The repertoire %1 cannot be read. %2").arg(QDir::toNativeSeparators(source), err);
            return false;
        }
        for (const Game &g : games)
            if (g.startPosition() == Position::startPosition())
                addTree(g, Game::RootNode, &tree);
        name = QFileInfo(source).completeBaseName();
        for (const training::Repertoire &r : training::repertoires())
            if (r.path == source)
                name = r.name;
    }
    if (!tree.contains(positionKey(Position::startPosition()))) {
        *error = tr("%1 has no moves from the initial position.").arg(name);
        return false;
    }

    m_kind = Kind::Opening;
    m_openingSource = source;
    m_openingName = name;
    m_openingTree = tree;
    m_userColor = color;
    m_start = Position::startPosition();
    m_taskPosition = m_start;
    m_offset = 0;
    m_puzzle = training::Puzzle();

    GameSettings g = baseSettings();
    g.builtinPlayer = GameSettings::BuiltinTrainer;
    g.enginePath.clear();
    g.opponentName = name;
    *settings = g;
    *start = m_start;
    return true;
}

QVector<QPair<QString, int>> Trainer::openingMoves(const Position &pos) const
{
    if (m_openingSource == QLatin1String("book") && pos.fullmoveNumber() > m_bookDepth)
        return {};
    return m_openingTree.value(positionKey(pos));
}

bool Trainer::openingLineEnds(const Position &pos) const
{
    return pos.status() != GameStatus::Ongoing || openingMoves(pos).isEmpty();
}

bool Trainer::prepareAgain(GameSettings *settings, Position *start, QString *error)
{
    if (m_kind == Kind::None) {
        *error = tr("There is no task.");
        return false;
    }
    GameSettings g = baseSettings();
    if (isPuzzle()) {
        g.builtinPlayer = GameSettings::BuiltinTrainer;
        g.opponentName = tr("Puzzle");
    } else {
        QString name;
        g.enginePath = m_kind == Kind::Resistance ? resistanceEnginePath(&name) : enginePath(&name);
        if (g.enginePath.isEmpty()) {
            *error = tr("No engine was found for the defence.");
            return false;
        }
        g.moveTimeMs = m_kind == Kind::Resistance ? m_resistSeconds * 1000 : 1000;
        if (m_kind == Kind::Resistance)
            g.opponentName = name;
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
    } else if (m_kind == Kind::Tactics) {
        m_message = tr("Watch the opponent's move, then find the best move.");
    } else if (m_kind == Kind::Opening) {
        m_message = m_userColor == White ? tr("Your move: play the first move of the repertoire.")
                                         : tr("Watch the opponent's move, then answer with the repertoire.");
        QSettings s;
        s.setValue(openingKey(m_openingSource, m_userColor, "tries"),
                   s.value(openingKey(m_openingSource, m_userColor, "tries"), 0).toInt() + 1);
    } else if (m_kind == Kind::Resistance) {
        m_resistMoves = 0;
        m_resistEval = Evaluation();
        m_message = m_userColor == White ? tr("Your move. Hold out as long as you can!")
                                         : tr("%1 begins. Hold out as long as you can!").arg(m_resistEngine);
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

int Trainer::solutionIndex() const
{
    return qMax(0, m_gm->game().nodePly(m_gm->mainLineEnd()) - m_offset);
}

int Trainer::userMovesDone() const
{
    const int plies = m_gm->game().nodePly(m_gm->mainLineEnd()) - m_offset;
    return plies <= 0 ? 0 : (plies + 1) / 2;
}

Move Trainer::defenderMove(const Position &pos)
{
    if (!isPuzzle() || !isOnBoard())
        return Move();
    if (m_kind == Kind::Opening) {
        // one of the repertoire's moves, by weight
        const QVector<QPair<QString, int>> moves = openingMoves(pos);
        int total = 0;
        for (const auto &m : moves)
            total += qMax(1, m.second);
        if (total == 0)
            return Move();
        int r = int(QRandomGenerator::global()->bounded(total));
        for (const auto &m : moves) {
            r -= qMax(1, m.second);
            if (r < 0)
                return parseUci(pos, m.first);
        }
        return Move();
    }
    if (m_offset && pos == m_start)
        return parseUci(pos, m_puzzle.firstMove);
    if (m_kind == Kind::Tactics) {
        // the opponent's moves of the Lichess solution
        const int i = solutionIndex();
        const Move m = i < m_puzzle.solution.size() ? parseUci(pos, m_puzzle.solution.at(i)) : Move();
        return !m.isNull() && pos.isLegal(m) ? m : Move();
    }
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
    if (!isRunning() || !isPuzzle())
        return true;
    const Position &pos = m_gm->game().nodePosition(m_gm->mainLineEnd());
    if (pos.sideToMove() != m_userColor || !pos.isLegal(move))
        return true;
    if (m_kind == Kind::Opening) {
        const QString uci = toUci(move);
        for (const auto &m : openingMoves(pos))
            if (m.first == uci) {
                m_message = tr("Correct! Go on.");
                emit changed();
                return true;
            }
        ++m_mistakes;
        m_message = m_openingSource == QLatin1String("book")
            ? tr("%1 is not in the book here. Try again.").arg(san(pos, move))
            : tr("%1 is not in the repertoire here. Try again.").arg(san(pos, move));
        emit changed();
        return false;
    }
    if (m_kind == Kind::Tactics) {
        // the move of the solution, or any mate (as on Lichess)
        const int i = solutionIndex();
        const QString expected = m_puzzle.solution.value(i);
        if (toUci(move) == expected || pos.after(move).isCheckmate()) {
            if (i + 1 < m_puzzle.solution.size())
                m_message = tr("Correct! Go on.");
            emit changed();
            return true;
        }
        ++m_mistakes;
        m_message = tr("%1 is not the best move. Try again.").arg(san(pos, move));
        emit changed();
        return false;
    }
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
    static const char *const pieces[] = {
            QT_TRANSLATE_NOOP("Trainer", "a pawn"), QT_TRANSLATE_NOOP("Trainer", "the knight"),
            QT_TRANSLATE_NOOP("Trainer", "the bishop"), QT_TRANSLATE_NOOP("Trainer", "the rook"),
            QT_TRANSLATE_NOOP("Trainer", "the queen"), QT_TRANSLATE_NOOP("Trainer", "the king"),
    };
    if (m_kind == Kind::Opening) {
        if (!isRunning() || pos.sideToMove() != m_userColor)
            return QString();
        const QVector<QPair<QString, int>> moves = openingMoves(pos);
        if (moves.isEmpty())
            return QString();
        const Move k = parseUci(pos, moves.constFirst().first);
        return tr("Move %1.").arg(tr(pieces[typeOf(pos.pieceAt(k.from()))]));
    }
    if (m_kind == Kind::Tactics) {
        if (!isRunning() || pos.sideToMove() != m_userColor)
            return QString();
        const Move k = parseUci(pos, m_puzzle.solution.value(solutionIndex()));
        if (k.isNull())
            return QString();
        QString text = tr("Move %1.").arg(tr(pieces[typeOf(pos.pieceAt(k.from()))]));
        QStringList themes;
        for (const QString &t : m_puzzle.themes)
            for (const training::TacticTheme &th : training::tacticThemes())
                if (t == QLatin1String(th.id) && t != m_theme)
                    themes << training::tacticThemeName(t);
        if (!themes.isEmpty())
            text += QLatin1Char(' ') + tr("Themes: %1.").arg(themes.join(QStringLiteral(", ")));
        return text;
    }
    if (m_kind == Kind::Mate) {
        if (!isRunning() || pos.sideToMove() != m_userColor)
            return QString();
        const int left = m_mateMoves - userMovesDone();
        const MoveList keys = m_solver.keyMoves(pos, qMax(1, left));
        if (keys.empty())
            return tr("There is no mate any more: take the last move back.");
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
    if (m_kind == Kind::Resistance)
        return tr("No help here: you are on your own against %1.").arg(m_resistEngine);
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
    if (!isPuzzle() || !isOnBoard())
        return QString();
    m_helped = true;
    const Position &pos = m_gm->game().nodePosition(m_gm->mainLineEnd());
    if (m_kind == Kind::Opening) {
        if (pos.sideToMove() != m_userColor)
            return QString();
        QStringList list;
        for (const auto &m : openingMoves(pos))
            list << san(pos, parseUci(pos, m.first));
        emit changed();
        if (list.isEmpty())
            return QString();
        return m_openingSource == QLatin1String("book") ? tr("Book moves: %1").arg(list.join(QStringLiteral(", ")))
                                                        : tr("Moves of the repertoire: %1").arg(list.join(QStringLiteral(", ")));
    }
    if (m_kind == Kind::Tactics) {
        m_message = tr("The puzzle comes again later.");
        emit changed();
        if (pos.sideToMove() != m_userColor)
            return sanLine(m_taskPosition, m_puzzle.solution);
        return sanLine(pos, m_puzzle.solution.mid(solutionIndex()));
    }
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
    if (m_kind == Kind::Tactics) {
        QStringList solved_ = s.value(tacticsKey("solved")).toStringList();
        QStringList retry = s.value(tacticsKey("retry")).toStringList();
        retry.removeAll(m_puzzle.id);
        if (solved && !m_helped && m_mistakes == 0) {
            if (!solved_.contains(m_puzzle.id))
                solved_ << m_puzzle.id;
        } else {
            retry << m_puzzle.id; // comes again after a few others
        }
        s.setValue(tacticsKey("solved"), solved_);
        s.setValue(tacticsKey("retry"), retry);
    }
    if (m_kind == Kind::Opening) {
        if (solved && !m_helped && m_mistakes == 0)
            s.setValue(openingKey(m_openingSource, m_userColor, "perfect"),
                       s.value(openingKey(m_openingSource, m_userColor, "perfect"), 0).toInt() + 1);
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
    if (isPuzzle()) {
        if (userWon)
            finish(true, m_mistakes || m_helped ? tr("Mate! Solved, but with help or mistakes: it comes again later.")
                                                : tr("Mate! Puzzle solved."));
        else
            finish(false, reason);
        return;
    }
    if (m_kind == Kind::Resistance) {
        if (userWon || result == Result::Draw) {
            m_resistMoves = userMovesDone();
            finishResistance(reason, true);
        } else {
            finishResistance(reason, false);
        }
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
        if (m_kind == Kind::Resistance)
            m_helped = true;
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
    if (m_kind == Kind::Resistance) {
        checkResistance(before);
        if (m_state != State::Running)
            return;
    }
    if (m_kind == Kind::Opening) {
        if (openingLineEnds(after)) {
            finish(true, m_mistakes || m_helped
                             ? tr("End of the line, but with help or mistakes. Practise it again!")
                             : tr("End of the line: all moves right!"));
            m_gm->stopPlaying();
            return;
        }
        if (after.sideToMove() == m_userColor)
            m_message = tr("Your move.");
    }
    if (m_kind == Kind::Tactics) {
        if (before.sideToMove() == m_userColor && solutionIndex() >= m_puzzle.solution.size()) {
            finish(true, m_mistakes || m_helped ? tr("Solved, but with help or mistakes: it comes again later.")
                                                : tr("Puzzle solved!"));
            m_gm->stopPlaying();
            return;
        }
        if (after.sideToMove() == m_userColor && ply == m_offset)
            m_message = tr("Your move: find the best move.");
    }
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
// Hold out

int Trainer::resistanceLimit()
{
    return qBound(1, QSettings().value(QStringLiteral("training/resistance/limit"), 2).toInt(), 5);
}

void Trainer::setResistanceLimit(int pawns)
{
    QSettings().setValue(QStringLiteral("training/resistance/limit"), pawns);
}

int Trainer::resistanceSeconds()
{
    return qBound(1, QSettings().value(QStringLiteral("training/resistance/seconds"), 2).toInt(), 10);
}

void Trainer::setResistanceSeconds(int s)
{
    QSettings().setValue(QStringLiteral("training/resistance/seconds"), s);
}

int Trainer::resistanceRecord(Color color, int limit)
{
    return QSettings().value(resistanceKey(color, limit, "record"), 0).toInt();
}

void Trainer::resetResistanceRecords()
{
    QSettings().remove(QStringLiteral("training/resistance/white"));
    QSettings().remove(QStringLiteral("training/resistance/black"));
}

QString Trainer::resistanceEnginePath(QString *name)
{
    // the strongest engine there is
    opponents::Opponent o;
    o.engines = QStringList{ QStringLiteral("Stockfish"), QStringLiteral("Arasan"), QStringLiteral("Texel"),
                             QStringLiteral("Patricia"), QStringLiteral("Toga II"), QStringLiteral("Fruit") };
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

bool Trainer::prepareResistance(Color color, GameSettings *settings, Position *start, QString *error)
{
    QString name;
    const QString path = resistanceEnginePath(&name);
    if (path.isEmpty()) {
        *error = tr("No engine was found. Engines > Manage engines adds one.");
        return false;
    }
    m_kind = Kind::Resistance;
    m_userColor = color;
    m_resistLimit = resistanceLimit();
    m_resistSeconds = resistanceSeconds();
    m_resistEngine = name;
    m_start = Position::startPosition();
    m_taskPosition = m_start;
    m_offset = 0;
    GameSettings g = baseSettings();
    g.enginePath = path;
    g.moveTimeMs = m_resistSeconds * 1000;
    g.opponentName = name;
    *settings = g;
    *start = m_start;
    return true;
}

void Trainer::checkResistance(const Position &before)
{
    // after a move of the engine: its evaluation of the position
    if (before.sideToMove() == m_userColor)
        return;
    const Game &game = m_gm->game();
    const Evaluation e = game.evaluation(m_gm->mainLineEnd());
    if (!e.valid)
        return;
    m_resistEval = e;
    const int sign = m_userColor == White ? 1 : -1;
    const bool lost = e.mate ? e.value * sign < 0 : e.value * sign < -100 * m_resistLimit;
    if (!lost) {
        m_resistMoves = userMovesDone();
        m_message = tr("Evaluation %1: you hold out.").arg(resistanceEvalText(e));
        return;
    }
    finishResistance(tr("The evaluation fell to %1.").arg(resistanceEvalText(e)), false);
    m_gm->stopPlaying();
}

QString Trainer::resistanceEvalText(const Evaluation &e) const
{
    const int v = m_userColor == White ? e.value : -e.value;
    if (e.mate)
        return v > 0 ? tr("mate in %n for you", nullptr, v) : tr("mate in %n against you", nullptr, -v);
    const QLocale locale(LanguageManager::savedLanguage() == QLatin1String("de") ? QLocale::German : QLocale::English);
    return (v > 0 ? QStringLiteral("+") : v < 0 ? QStringLiteral("\u2212") : QString())
        + locale.toString(qAbs(v) / 100.0, 'f', 2);
}

void Trainer::finishResistance(const QString &why, bool wholeGame)
{
    const int moves = m_resistMoves;
    const int record = resistanceRecord(m_userColor, m_resistLimit);
    QString text = why + QLatin1Char(' ')
        + (wholeGame ? tr("You held out through the whole game: %n move(s)!", nullptr, moves)
                     : tr("You held out %n move(s).", nullptr, moves));
    const bool newRecord = !m_helped && moves > record;
    if (m_helped)
        text += QLatin1Char(' ') + tr("Moves were taken back: no record.");
    else if (newRecord)
        text += QLatin1Char(' ') + tr("A new record!");
    if (newRecord)
        QSettings().setValue(resistanceKey(m_userColor, m_resistLimit, "record"), moves);
    finish(newRecord || wholeGame, text);
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
    if (m_kind == Kind::Tactics) {
        //: panel Training: "Tactics: Fork - puzzle 15 of 378"
        return tr("Tactics: %1 - puzzle %2 of %3")
            .arg(training::tacticThemeName(m_theme))
            .arg(m_puzzleIndex + 1)
            .arg(training::tacticIndexes(m_theme).size());
    }
    if (m_kind == Kind::Endgame)
        return endgameName(m_endgame);
    if (m_kind == Kind::Opening)
        return tr("Opening: %1").arg(m_openingName);
    if (m_kind == Kind::Resistance)
        return tr("Hold out against %1").arg(m_resistEngine);
    return tr("Training");
}

QString Trainer::task() const
{
    if (m_kind == Kind::Mate)
        return tr("%1 to move mates in %n move(s).", nullptr, m_mateMoves).arg(colorName(m_userColor));
    if (m_kind == Kind::Tactics)
        return tr("%1 to move: find the best move and play the whole combination.").arg(colorName(m_userColor));
    if (m_kind == Kind::Opening)
        return m_openingSource == QLatin1String("book")
            ? tr("Play the opening as %1 with moves of the book, up to move %2.").arg(colorName(m_userColor)).arg(m_bookDepth)
            : tr("Play the repertoire as %1 until the line ends; mbChess plays one of its other lines.")
                  .arg(colorName(m_userColor));
    if (m_kind == Kind::Resistance)
        return tr("Play as %1 against %2 at full strength (%n s per move). The game lasts as long as the "
                  "engine's evaluation stays above -%3 pawns for you.", nullptr, m_resistSeconds)
            .arg(colorName(m_userColor), m_resistEngine)
            .arg(m_resistLimit);
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
    if (!isPuzzle() || m_kind == Kind::Opening)
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
    if (m_kind == Kind::Tactics) {
        const QSet<QString> solved = settingsSet(tacticsKey("solved"));
        int done = 0;
        const QVector<int> idx = training::tacticIndexes(m_theme);
        for (int i : idx)
            if (solved.contains(training::tacticPuzzles().at(i).id))
                ++done;
        QString text = tr("Solved: %1 of %2").arg(done).arg(idx.size());
        const int retry = s.value(tacticsKey("retry")).toStringList().size();
        if (retry > 0)
            text += QStringLiteral(" · ") + tr("to repeat: %1").arg(retry);
        if (m_mistakes > 0)
            text += QStringLiteral(" · ") + tr("mistakes in this puzzle: %1").arg(m_mistakes);
        return text;
    }
    if (m_kind == Kind::Opening) {
        QString text = tr("Lines without a mistake: %1 of %2")
                           .arg(s.value(openingKey(m_openingSource, m_userColor, "perfect"), 0).toInt())
                           .arg(s.value(openingKey(m_openingSource, m_userColor, "tries"), 0).toInt());
        if (m_mistakes > 0)
            text += QStringLiteral(" · ") + tr("mistakes in this line: %1").arg(m_mistakes);
        return text;
    }
    if (m_kind == Kind::Resistance)
        return tr("Moves held out: %1 · record: %2")
            .arg(m_resistMoves)
            .arg(resistanceRecord(m_userColor, m_resistLimit));
    if (m_kind == Kind::Endgame)
        return tr("Won: %1 of %2 tries")
            .arg(s.value(endgameKey(m_endgame, "solved"), 0).toInt())
            .arg(s.value(endgameKey(m_endgame, "tries"), 0).toInt());
    return QString();
}
