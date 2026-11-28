// SPDX-License-Identifier: GPL-3.0-or-later
#include "tutordialog.h"

#include "languagemanager.h"

#include <chesscore/notation.h>

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLocale>
#include <QPixmap>
#include <QPushButton>
#include <QVBoxLayout>

using namespace mbc;

namespace {

QLocale uiLocale()
{
    return QLocale(LanguageManager::savedLanguage() == QLatin1String("de") ? QLocale::German : QLocale::English);
}

// "+0,4", "-2,8", "mate in 3 for you", from the player's view
QString evalText(const Evaluation &e, Color side)
{
    if (!e.valid)
        return QStringLiteral("?");
    const int sign = side == White ? 1 : -1;
    const int v = e.value * sign;
    if (e.mate) {
        if (v > 0)
            return TutorDialog::tr("mate in %n for you", nullptr, v);
        return TutorDialog::tr("mate in %n for your opponent", nullptr, -v);
    }
    const QString number = uiLocale().toString(qAbs(v) / 100.0, 'f', 1);
    return (v > 0 ? QStringLiteral("+") : v < 0 ? QStringLiteral("−") : QString()) + number;
}

// "13. Nf3 Nxe4 14. Qxe4", at most `plies` half-moves
QString sanLine(const Position &start, const QStringList &uci, int plies, const QString &letters)
{
    Position pos = start;
    QString out;
    for (int i = 0; i < uci.size() && i < plies; ++i) {
        const Move m = parseUci(pos, uci.at(i));
        if (m.isNull())
            break;
        if (pos.sideToMove() == White)
            out += QStringLiteral("%1. ").arg(pos.fullmoveNumber());
        else if (i == 0)
            out += QStringLiteral("%1... ").arg(pos.fullmoveNumber());
        out += localizeSan(toSan(pos, m), letters) + QLatin1Char(' ');
        pos.makeMove(m);
    }
    return out.trimmed();
}

QString capturedText(PieceType t, const QString &san)
{
    switch (t) {
    case Queen: return TutorDialog::tr("%1 takes your queen.").arg(san);
    case Rook: return TutorDialog::tr("%1 takes your rook.").arg(san);
    case Bishop: return TutorDialog::tr("%1 takes your bishop.").arg(san);
    case Knight: return TutorDialog::tr("%1 takes your knight.").arg(san);
    case Pawn: return TutorDialog::tr("%1 takes your pawn.").arg(san);
    default: return QString();
    }
}

} // namespace

QString TutorDialog::explanation(const Tutor::Verdict &v, const QString &letters)
{
    Position before;
    before.setFromFen(v.fen);
    const Color side = before.sideToMove();
    const QString san = localizeSan(toSan(before, v.move), letters);
    const Position after = before.after(v.move);

    QStringList parts;
    if (v.severity == Tutor::Stoned)
        parts << QStringLiteral("<b>") + tr("Are you stoned?!") + QStringLiteral("</b>");
    switch (v.severity) {
    case Tutor::Inaccuracy: parts << tr("<b>%1</b> is not exact.").arg(san); break;
    case Tutor::Mistake: parts << tr("<b>%1</b> is a mistake.").arg(san); break;
    case Tutor::Blunder: parts << tr("<b>%1</b> is a blunder.").arg(san); break;
    case Tutor::Stoned: parts << tr("<b>%1</b> is a terrible blunder.").arg(san); break;
    default: break;
    }
    parts << tr("Your position drops from %1 to %2.").arg(evalText(v.before, side), evalText(v.after, side));
    QString text = QStringLiteral("<p>") + parts.join(QLatin1Char(' ')) + QStringLiteral("</p>");

    // why: the opponent's answer
    if (!v.reply.isEmpty()) {
        const QString line = sanLine(after, v.reply, 5, letters);
        const Move reply = parseUci(after, v.reply.constFirst());
        QString why;
        const int sign = side == White ? 1 : -1;
        if (v.after.mate && v.after.value * sign < 0) {
            why = tr("Then your opponent mates you: %1").arg(line);
        } else if (!reply.isNull() && after.isCapture(reply) && after.pieceAt(reply.to()) != NoPiece) {
            why = capturedText(typeOf(after.pieceAt(reply.to())), localizeSan(toSan(after, reply), letters));
            if (v.reply.size() > 1)
                why += QLatin1Char(' ') + tr("For example: %1").arg(line);
        } else {
            why = tr("The best answer is: %1").arg(line);
        }
        text += QStringLiteral("<p>") + why + QStringLiteral("</p>");
    }
    // what is better
    if (!v.best.isNull()) {
        QString better = tr("Better is <b>%1</b> (%2), for example %3.")
                             .arg(localizeSan(toSan(before, v.best), letters), evalText(v.before, side),
                                  sanLine(before, v.bestLine, 4, letters));
        const int sign = side == White ? 1 : -1;
        if (v.before.mate && v.before.value * sign > 0)
            better = tr("You had a mate in %n!", nullptr, v.before.value * sign) + QLatin1Char(' ') + better;
        text += QStringLiteral("<p>") + better + QStringLiteral("</p>");
    }
    return text;
}

TutorDialog::TutorDialog(const Tutor::Verdict &verdict, const QString &pieceLetters, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Tutor: %1").arg(Tutor::severityName(verdict.severity)));

    auto *picture = new QLabel(this);
    QPixmap pixmap(verdict.severity == Tutor::Stoned ? QStringLiteral(":/images/tutor-gekifft.png")
                                                     : QStringLiteral(":/images/tutor.png"));
    const qreal dpr = devicePixelRatioF();
    if (!pixmap.isNull()) {
        pixmap = pixmap.scaledToHeight(qRound(280 * dpr), Qt::SmoothTransformation);
        pixmap.setDevicePixelRatio(dpr);
        picture->setPixmap(pixmap);
    }
    picture->setFrameShape(QFrame::StyledPanel);

    auto *title = new QLabel(Tutor::severityName(verdict.severity), this);
    QFont f = title->font();
    f.setPointSizeF(f.pointSizeF() * 1.4);
    f.setBold(true);
    title->setFont(f);

    auto *text = new QLabel(explanation(verdict, pieceLetters), this);
    text->setTextFormat(Qt::RichText);
    text->setWordWrap(true);
    text->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    text->setMinimumWidth(340);

    auto *takeBack = new QPushButton(tr("&Take back and think again"), this);
    auto *best = new QPushButton(tr("Play the &tutor's move"), this);
    auto *anyway = new QPushButton(tr("&Play my move anyway"), this);
    best->setEnabled(!verdict.best.isNull());
    takeBack->setDefault(true);
    connect(takeBack, &QPushButton::clicked, this, [this] { m_choice = TakeBack; accept(); });
    connect(best, &QPushButton::clicked, this, [this] { m_choice = PlayBest; accept(); });
    connect(anyway, &QPushButton::clicked, this, [this] { m_choice = PlayAnyway; accept(); });

    auto *buttons = new QVBoxLayout;
    buttons->addWidget(takeBack);
    buttons->addWidget(best);
    buttons->addWidget(anyway);

    auto *right = new QVBoxLayout;
    right->addWidget(title);
    right->addWidget(text, 1);
    right->addLayout(buttons);
    auto *layout = new QHBoxLayout(this);
    layout->setSpacing(16);
    layout->addWidget(picture, 0, Qt::AlignTop);
    layout->addLayout(right, 1);
}
