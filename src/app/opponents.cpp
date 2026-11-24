// SPDX-License-Identifier: GPL-3.0-or-later
#include "opponents.h"

#include "enginemanager.h"

#include <QCoreApplication>
#include <QRandomGenerator>

using namespace mbc;

namespace opponents {

namespace {

QString tr(const char *text)
{
    return QCoreApplication::translate("opponents", text);
}

Opponent make(const char *name, const char *style, int elo, int engineElo, QStringList engines,
              QMap<QString, QString> options = {}, int builtin = GameSettings::NoBuiltin)
{
    Opponent o;
    o.name = name;
    o.style = style;
    o.elo = elo;
    o.engineElo = engineElo;
    o.engines = engines;
    o.options = options;
    o.builtin = builtin;
    return o;
}

} // namespace

QVector<Opponent> list(Group group)
{
    // Texel can be held down to below 0 Elo (UCI_Elo -625..2900, calibrated by its
    // author), Patricia makes human-looking mistakes (Skill_Level), Stockfish starts at
    // 1320 Elo, Arasan at 1000.
    const QStringList texel{ QStringLiteral("Texel"), QStringLiteral("Patricia") };
    const QStringList patricia{ QStringLiteral("Patricia"), QStringLiteral("Texel") };
    switch (group) {
    case Group::Beginner:
        return {
            make(QT_TRANSLATE_NOOP("opponents", "Random mover"),
                 QT_TRANSLATE_NOOP("opponents", "Plays any legal move. For learning how the pieces move and capture. "
                                                "Played by mbChess itself, without an engine."),
                 100, 0, {}, {}, GameSettings::BuiltinRandom),
            make(QT_TRANSLATE_NOOP("opponents", "Piece hunter"),
                 QT_TRANSLATE_NOOP("opponents", "Captures whenever it can, the most valuable piece first, even when "
                                                "the piece is defended. For learning to set traps. Played by mbChess "
                                                "itself, without an engine."),
                 250, 0, {}, {}, GameSettings::BuiltinCapturer),
            make(QT_TRANSLATE_NOOP("opponents", "Novice"),
                 QT_TRANSLATE_NOOP("opponents", "Often misses threats of a single move."), 400, 400, texel),
            make(QT_TRANSLATE_NOOP("opponents", "Beginner"),
                 QT_TRANSLATE_NOOP("opponents", "Usually protects its pieces, but misses forks and skewers."), 600, 600,
                 texel),
            make(QT_TRANSLATE_NOOP("opponents", "Leisure player"),
                 QT_TRANSLATE_NOOP("opponents", "Likes to attack and makes human mistakes."), 800, 800, patricia),
            make(QT_TRANSLATE_NOOP("opponents", "Advanced beginner"),
                 QT_TRANSLATE_NOOP("opponents", "Plays actively, loses a piece now and then."), 1000, 1000, patricia),
            make(QT_TRANSLATE_NOOP("opponents", "Hobby player"),
                 QT_TRANSLATE_NOOP("opponents", "Solid; mistakes only in longer variations."), 1200, 1200, texel),
        };
    case Group::Computer: {
        const QStringList strong{ QStringLiteral("Stockfish"), QStringLiteral("Arasan"), QStringLiteral("Texel") };
        return {
            make(QT_TRANSLATE_NOOP("opponents", "Casual player"),
                 QT_TRANSLATE_NOOP("opponents", "Knows the basic rules of strategy, still overlooks tactics."), 1600, 1600,
                 strong),
            make(QT_TRANSLATE_NOOP("opponents", "Club player"),
                 QT_TRANSLATE_NOOP("opponents", "A solid club player."), 1900, 1900, strong),
            make(QT_TRANSLATE_NOOP("opponents", "Expert"),
                 QT_TRANSLATE_NOOP("opponents", "A strong club player."), 2200, 2200, strong),
            make(QT_TRANSLATE_NOOP("opponents", "Master"),
                 QT_TRANSLATE_NOOP("opponents", "Plays like a master."), 2500, 2500, strong),
            make(QT_TRANSLATE_NOOP("opponents", "Full strength"),
                 QT_TRANSLATE_NOOP("opponents", "The engine at full strength: far stronger than any human."), 0, 0,
                 strong),
        };
    }
    case Group::Character:
        return {
            make(QT_TRANSLATE_NOOP("opponents", "Attacker"),
                 QT_TRANSLATE_NOOP("opponents", "Plays for the attack on the king and sacrifices material for it."), 1800,
                 1800, patricia),
            make(QT_TRANSLATE_NOOP("opponents", "Positional player"),
                 QT_TRANSLATE_NOOP("opponents", "Plays calmly, improves its pieces and waits for your mistakes."), 1800,
                 1800, { QStringLiteral("Arasan"), QStringLiteral("Texel"), QStringLiteral("Stockfish") }),
            make(QT_TRANSLATE_NOOP("opponents", "Materialist"),
                 QT_TRANSLATE_NOOP("opponents", "Grabs every pawn it can get and defends it."), 1800, 1800,
                 { QStringLiteral("Texel") }, { { QStringLiteral("Materialistic"), QStringLiteral("true") } }),
            make(QT_TRANSLATE_NOOP("opponents", "Classic"),
                 QT_TRANSLATE_NOOP("opponents", "A classic engine of the 2000s without a neural network, at full "
                                                "strength: plays clear, principled chess."),
                 2600, 0, { QStringLiteral("Fruit"), QStringLiteral("Toga II"), QStringLiteral("Glaurung") }),
        };
    }
    return {};
}

QString displayName(const Opponent &o)
{
    return tr(o.name);
}

QString menuText(const Opponent &o)
{
    return o.elo > 0 ? tr("%1 (about %2 Elo)").arg(displayName(o)).arg(o.elo) : displayName(o);
}

QString enginePath(const Opponent &o, QString *engineName)
{
    if (o.builtin != GameSettings::NoBuiltin)
        return QString();
    const QVector<EngineEntry> engines = enginemanager::engines();
    for (const QString &wanted : o.engines)
        for (const EngineEntry &e : engines)
            if (e.inFolder && e.defaultName.compare(wanted, Qt::CaseInsensitive) == 0) {
                if (engineName)
                    *engineName = e.name;
                return e.path;
            }
    return QString();
}

bool isAvailable(const Opponent &o)
{
    return o.builtin != GameSettings::NoBuiltin || !enginePath(o).isEmpty();
}

QString toolTip(const Opponent &o)
{
    QString text = tr(o.style);
    if (o.builtin != GameSettings::NoBuiltin)
        return text;
    QString name;
    if (enginePath(o, &name).isEmpty())
        return text + QLatin1Char('\n') + tr("Not available: the engine %1 is missing.").arg(o.engines.value(0));
    text += QLatin1Char('\n') + tr("Engine: %1").arg(name);
    if (!o.engines.isEmpty() && name.compare(o.engines.first(), Qt::CaseInsensitive) != 0)
        text += QLatin1Char(' ') + tr("(instead of %1, which is missing)").arg(o.engines.first());
    return text;
}

GameSettings quickSettings(Group group, const Opponent &o)
{
    GameSettings g = GameSettings::load(); // colour, clock, name, threads, hash, books
    bool white = g.colorChoice != GameSettings::PlayBlack;
    if (g.colorChoice == GameSettings::PlayRandom)
        white = QRandomGenerator::global()->bounded(2) == 0;
    g.mode = white ? GameSettings::HumanWhiteVsEngine : GameSettings::HumanBlackVsEngine;
    g.enginePath = enginePath(o);
    g.engine2Path.clear();
    g.strengthElo = o.engineElo;
    g.engineOptions = o.options;
    g.builtinPlayer = o.builtin;
    g.opponentName = displayName(o);
    g.opponentElo = o.elo;
    if (group == Group::Beginner) {
        // no clock, no opening books: a beginner should not play master openings
        g.timeControl = TimeControl();
        g.useBook = false;
        g.useChessbook = false;
        g.engineOwnBook = false;
        g.threads = 1;
    }
    return g;
}

Move builtinMove(const Position &pos, int builtin)
{
    const MoveList moves = pos.legalMoves();
    if (moves.empty())
        return Move();
    auto *rng = QRandomGenerator::global();
    if (builtin == GameSettings::BuiltinCapturer) {
        // the most valuable piece it can take (a promotion counts as a gain too)
        static const int value[] = { 1, 3, 3, 5, 9, 0, 0 };
        int best = 0;
        MoveList candidates;
        for (const Move &m : moves) {
            int gain = 0;
            if (m.isEnPassant())
                gain = 1;
            else if (pos.pieceAt(m.to()) != NoPiece)
                gain = value[typeOf(pos.pieceAt(m.to()))];
            if (m.isPromotion())
                gain += value[m.promotion()] - 1;
            if (gain > best) {
                best = gain;
                candidates.clear();
            }
            if (gain == best && gain > 0)
                candidates.push_back(m);
        }
        if (!candidates.empty())
            return candidates[rng->bounded(int(candidates.size()))];
    }
    return moves[rng->bounded(int(moves.size()))];
}

} // namespace opponents
