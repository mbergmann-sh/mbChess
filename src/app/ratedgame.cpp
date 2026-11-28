// SPDX-License-Identifier: GPL-3.0-or-later
#include "ratedgame.h"

#include "languagemanager.h"
#include "opponents.h"

#include <QComboBox>
#include <QCoreApplication>
#include <QDate>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QListWidget>
#include <QLocale>
#include <QMessageBox>
#include <QPushButton>
#include <QRandomGenerator>
#include <QSettings>
#include <QVBoxLayout>
#include <QtMath>

namespace rated {

namespace {

const int kStartElo = 1200;
const int kMinElo = 100;
const int kMaxHistory = 50;

struct TimeChoice
{
    int minutes, increment;
};
const TimeChoice kTimes[] = { { 5, 3 }, { 10, 5 }, { 15, 10 }, { 30, 0 }, { 0, 0 } };
const int kDefaultTime = 2;

QString key(const char *name)
{
    return QStringLiteral("rated/") + QLatin1String(name);
}

QString encode(const Record &r)
{
    return QStringList{ r.date, r.opponent, QString::number(r.opponentElo), r.white ? QStringLiteral("w") : QStringLiteral("b"),
                        QString::number(r.score), QString::number(r.change), QString::number(r.eloAfter),
                        r.abandoned ? QStringLiteral("a") : QString() }
        .join(QLatin1Char('|'));
}

Record decode(const QString &s)
{
    const QStringList f = s.split(QLatin1Char('|'));
    Record r;
    r.date = f.value(0);
    r.opponent = f.value(1);
    r.opponentElo = f.value(2).toInt();
    r.white = f.value(3) != QLatin1String("b");
    r.score = f.value(4).toDouble();
    r.change = f.value(5).toInt();
    r.eloAfter = f.value(6).toInt();
    r.abandoned = f.value(7) == QLatin1String("a");
    return r;
}

// The engine and the rating it really plays at (Stockfish starts at 1320, Arasan at 1000)
QString engineFor(int elo, QString *name, int *effective)
{
    opponents::Opponent o;
    o.engines = elo >= 2900 ? QStringList{ QStringLiteral("Stockfish"), QStringLiteral("Texel"), QStringLiteral("Arasan") }
                            : QStringList{ QStringLiteral("Texel"), QStringLiteral("Stockfish"), QStringLiteral("Arasan"),
                                           QStringLiteral("Patricia") };
    const QString path = opponents::enginePath(o, name);
    int e = elo;
    if (name->contains(QLatin1String("Stockfish"), Qt::CaseInsensitive))
        e = qBound(1320, elo, 3190);
    else if (name->contains(QLatin1String("Arasan"), Qt::CaseInsensitive))
        e = qBound(1000, elo, 3000);
    else if (name->contains(QLatin1String("Texel"), Qt::CaseInsensitive))
        e = qMin(elo, 2900);
    *effective = e;
    return path;
}

} // namespace

int elo()
{
    return QSettings().value(key("elo"), kStartElo).toInt();
}

int games()
{
    return QSettings().value(key("games"), 0).toInt();
}

QVector<Record> history()
{
    QVector<Record> list;
    for (const QString &s : QSettings().value(key("history")).toStringList())
        list << decode(s);
    return list;
}

void reset()
{
    QSettings().remove(QStringLiteral("rated"));
}

bool hasPending()
{
    return !QSettings().value(key("pending")).toString().isEmpty();
}

bool prepare(GameSettings::ColorChoice color, const TimeControl &tc, GameSettings *settings, QString *error)
{
    QString name;
    int effective = 0;
    const QString path = engineFor(elo(), &name, &effective);
    if (path.isEmpty()) {
        *error = QCoreApplication::translate("RatedGameDialog",
                                             "No engine was found. Engines > Manage engines adds one.");
        return false;
    }
    GameSettings g = GameSettings::load(); // name, threads, hash, books
    bool white = color == GameSettings::PlayWhite;
    if (color == GameSettings::PlayRandom)
        white = QRandomGenerator::global()->bounded(2) == 0;
    g.colorChoice = color;
    g.mode = white ? GameSettings::HumanWhiteVsEngine : GameSettings::HumanBlackVsEngine;
    g.enginePath = path;
    g.engine2Path.clear();
    g.strengthElo = effective;
    g.opponentElo = effective;
    g.engineOptions.clear();
    g.builtinPlayer = GameSettings::NoBuiltin;
    g.opponentName = name; // the window adds "(about 1200 Elo)"
    if (effective < 1600) {
        // no opening books for weaker opponents, as for the beginners of the menu Play
        g.useBook = false;
        g.useChessbook = false;
        g.engineOwnBook = false;
    }
    g.timeControl = tc;
    g.moveTimeMs = 0;
    g.ponder = false;
    *settings = g;
    return true;
}

void begin(const GameSettings &settings)
{
    Record r;
    r.date = QDate::currentDate().toString(Qt::ISODate);
    r.opponent = settings.opponentName;
    r.opponentElo = settings.opponentElo;
    r.white = settings.mode == GameSettings::HumanWhiteVsEngine;
    QSettings().setValue(key("pending"), encode(r));
}

Record finish(double score, bool abandoned)
{
    QSettings s;
    Record r = decode(s.value(key("pending")).toString());
    s.remove(key("pending"));
    const int before = elo();
    const int n = games();
    const double expected = 1.0 / (1.0 + qPow(10.0, (r.opponentElo - before) / 400.0));
    const int k = n < 20 ? 40 : 20;
    const int after = qMax(kMinElo, before + qRound(k * (score - expected)));
    r.score = score;
    r.change = after - before;
    r.eloAfter = after;
    r.abandoned = abandoned;
    s.setValue(key("elo"), after);
    s.setValue(key("games"), n + 1);
    QStringList list = s.value(key("history")).toStringList();
    list.prepend(encode(r));
    while (list.size() > kMaxHistory)
        list.removeLast();
    s.setValue(key("history"), list);
    const char *counter = score > 0.75 ? "wins" : score > 0.25 ? "draws" : "losses";
    s.setValue(key(counter), s.value(key(counter), 0).toInt() + 1);
    return r;
}

bool resolvePending(Record *record)
{
    if (!hasPending())
        return false;
    *record = finish(0, true);
    return true;
}

QString scoreText(double score)
{
    return score > 0.75 ? QCoreApplication::translate("RatedGameDialog", "won")
        : score > 0.25  ? QCoreApplication::translate("RatedGameDialog", "draw")
                        : QCoreApplication::translate("RatedGameDialog", "lost");
}

} // namespace rated

namespace {

QLocale uiLocale()
{
    return QLocale(LanguageManager::savedLanguage() == QLatin1String("de") ? QLocale::German : QLocale::English);
}

QString opponentText(const rated::Record &r)
{
    return QStringLiteral("%1 (%2 Elo)").arg(r.opponent).arg(r.opponentElo);
}

QString signedNumber(int v)
{
    return v > 0 ? QStringLiteral("+%1").arg(v) : v < 0 ? QStringLiteral("−%1").arg(-v) : QStringLiteral("±0");
}

} // namespace

RatedGameDialog::RatedGameDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Rated game"));
    QSettings s;

    m_elo = new QLabel(this);
    QFont f = m_elo->font();
    f.setPointSizeF(f.pointSizeF() * 1.6);
    f.setBold(true);
    m_elo->setFont(f);
    m_stats = new QLabel(this);
    auto *about = new QLabel(tr("You play against an engine held to your rating. The result changes the rating "
                                "(Elo formula; in the first 20 games by up to 40 points, then by up to 20). "
                                "Taking back moves and the tutor are off; a game left unfinished counts as lost."),
                             this);
    about->setWordWrap(true);
    m_history = new QListWidget(this);
    m_history->setMinimumHeight(140);

    m_color = new QComboBox(this);
    m_color->addItem(tr("White"), int(GameSettings::PlayWhite));
    m_color->addItem(tr("Black"), int(GameSettings::PlayBlack));
    m_color->addItem(tr("Random"), int(GameSettings::PlayRandom));
    m_color->setCurrentIndex(qBound(0, s.value(QStringLiteral("rated/color"), 2).toInt(), 2));
    m_time = new QComboBox(this);
    for (const rated::TimeChoice &t : rated::kTimes) {
        TimeControl tc;
        tc.baseMs = qint64(t.minutes) * 60000;
        tc.incrementMs = qint64(t.increment) * 1000;
        m_time->addItem(tc.label());
    }
    const int timeCount = int(sizeof(rated::kTimes) / sizeof(rated::kTimes[0]));
    m_time->setCurrentIndex(qBound(0, s.value(QStringLiteral("rated/time"), rated::kDefaultTime).toInt(), timeCount - 1));

    auto *form = new QFormLayout;
    form->addRow(tr("&Colour:"), m_color);
    form->addRow(tr("&Time control:"), m_time);

    auto *buttons = new QDialogButtonBox(this);
    QPushButton *start = buttons->addButton(tr("&Start the game"), QDialogButtonBox::AcceptRole);
    QPushButton *resetButton = buttons->addButton(tr("&Reset rating..."), QDialogButtonBox::ResetRole);
    buttons->addButton(QDialogButtonBox::Close);
    start->setDefault(true);
    connect(start, &QPushButton::clicked, this, &RatedGameDialog::startGame);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(resetButton, &QPushButton::clicked, this, [this] {
        if (QMessageBox::question(this, tr("Rated game"),
                                  tr("Start again with %1 Elo and forget all rated games?").arg(rated::kStartElo))
            == QMessageBox::Yes) {
            rated::reset();
            updateTexts();
        }
    });

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_elo);
    layout->addWidget(m_stats);
    layout->addWidget(about);
    layout->addWidget(m_history, 1);
    layout->addLayout(form);
    layout->addWidget(buttons);
    resize(520, 480);
    updateTexts();
}

void RatedGameDialog::updateTexts()
{
    QSettings s;
    m_elo->setText(tr("Your rating: %1 Elo").arg(rated::elo()));
    const int n = rated::games();
    m_stats->setText(n == 0 ? tr("No rated games yet.")
                            : tr("%n rated game(s): %1 won, %2 drawn, %3 lost", nullptr, n)
                                  .arg(s.value(QStringLiteral("rated/wins"), 0).toInt())
                                  .arg(s.value(QStringLiteral("rated/draws"), 0).toInt())
                                  .arg(s.value(QStringLiteral("rated/losses"), 0).toInt()));
    m_history->clear();
    const QLocale locale = uiLocale();
    for (const rated::Record &r : rated::history()) {
        const QString date = locale.toString(QDate::fromString(r.date, Qt::ISODate), QLocale::ShortFormat);
        QString text = tr("%1  %2 against %3: %4, %5 → %6")
                           .arg(date, r.white ? tr("White") : tr("Black"), opponentText(r), rated::scoreText(r.score),
                                signedNumber(r.change))
                           .arg(r.eloAfter);
        if (r.abandoned)
            text += QLatin1Char(' ') + tr("(not finished)");
        auto *item = new QListWidgetItem(text, m_history);
        item->setForeground(r.score > 0.75 ? QColor(40, 140, 40) : r.score > 0.25 ? QColor(150, 120, 0)
                                                                                 : QColor(200, 40, 40));
    }
}

void RatedGameDialog::startGame()
{
    QSettings s;
    s.setValue(QStringLiteral("rated/color"), m_color->currentIndex());
    s.setValue(QStringLiteral("rated/time"), m_time->currentIndex());
    const rated::TimeChoice &t = rated::kTimes[m_time->currentIndex()];
    TimeControl tc;
    tc.baseMs = qint64(t.minutes) * 60000;
    tc.incrementMs = qint64(t.increment) * 1000;
    QString error;
    if (!rated::prepare(GameSettings::ColorChoice(m_color->currentData().toInt()), tc, &m_settings, &error)) {
        QMessageBox::warning(this, tr("Rated game"), error);
        return;
    }
    accept();
}

QString RatedGameDialog::resultText(const rated::Record &r)
{
    const int before = r.eloAfter - r.change;
    QString head = r.abandoned ? tr("The rated game against %1 was not finished: it counts as lost.").arg(opponentText(r))
        : r.score > 0.75       ? tr("You won the rated game against %1!").arg(opponentText(r))
        : r.score > 0.25       ? tr("The rated game against %1 is drawn.").arg(opponentText(r))
                               : tr("You lost the rated game against %1.").arg(opponentText(r));
    return head + QStringLiteral("\n\n")
        + tr("Your rating: %1 → %2 (%3)").arg(before).arg(r.eloAfter).arg(signedNumber(r.change));
}
