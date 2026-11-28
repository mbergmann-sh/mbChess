// SPDX-License-Identifier: GPL-3.0-or-later
#include "betaexpiry.h"

#include "languagemanager.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLocale>
#include <QMessageBox>
#include <QSaveFile>
#include <QSettings>
#include <QStandardPaths>
#include <QTextStream>

#ifndef MBCHESS_BETA_DAYS
#define MBCHESS_BETA_DAYS 0
#endif
#ifndef MBCHESS_BETA_URL
#define MBCHESS_BETA_URL ""
#endif

namespace beta {

namespace {

const int kReminderDays = 7;

QString version()
{
    return QCoreApplication::applicationVersion();
}

QString settingsGroup()
{
    return QStringLiteral("beta/") + version();
}

QString filePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + QStringLiteral("/beta-")
        + version() + QStringLiteral(".txt");
}

struct Record
{
    QDate firstStart;
    QDate lastSeen;
};

// the earlier first start and the later last start of both places
Record load()
{
    Record r;
    QSettings s;
    r.firstStart = QDate::fromString(s.value(settingsGroup() + QStringLiteral("/firstStart")).toString(), Qt::ISODate);
    r.lastSeen = QDate::fromString(s.value(settingsGroup() + QStringLiteral("/lastSeen")).toString(), Qt::ISODate);
    QFile f(filePath());
    if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&f);
        const QDate first = QDate::fromString(in.readLine().trimmed(), Qt::ISODate);
        const QDate last = QDate::fromString(in.readLine().trimmed(), Qt::ISODate);
        if (first.isValid() && (!r.firstStart.isValid() || first < r.firstStart))
            r.firstStart = first;
        if (last.isValid() && (!r.lastSeen.isValid() || last > r.lastSeen))
            r.lastSeen = last;
    }
    return r;
}

void save(const Record &r)
{
    QSettings s;
    s.setValue(settingsGroup() + QStringLiteral("/firstStart"), r.firstStart.toString(Qt::ISODate));
    s.setValue(settingsGroup() + QStringLiteral("/lastSeen"), r.lastSeen.toString(Qt::ISODate));
    QDir().mkpath(QFileInfo(filePath()).absolutePath());
    QSaveFile f(filePath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&f);
        out << r.firstStart.toString(Qt::ISODate) << '\n' << r.lastSeen.toString(Qt::ISODate) << '\n';
        f.commit();
    }
}

// today, but never before the last start (clock set back)
QDate today(const Record &r)
{
    const QDate d = QDate::currentDate();
    return r.lastSeen.isValid() && d < r.lastSeen ? r.lastSeen : d;
}

QString dateText(const QDate &d)
{
    // in the language of the program, not of the system
    const QLocale locale(LanguageManager::savedLanguage() == QLatin1String("de") ? QLocale::German : QLocale::English);
    return locale.toString(d, QLocale::LongFormat);
}

QString downloadSentence()
{
    const QString url = downloadUrl();
    if (url.isEmpty())
        return QCoreApplication::translate("Beta", "You get the current build from the developer.");
    return QCoreApplication::translate("Beta", "You get the current build here: <a href=\"%1\">%1</a>").arg(url.toHtmlEscaped());
}

void message(QWidget *parent, QMessageBox::Icon icon, const QString &title, const QString &html)
{
    QMessageBox box(icon, title, html, QMessageBox::Ok, parent);
    box.setTextFormat(Qt::RichText);
    box.setTextInteractionFlags(Qt::TextBrowserInteraction);
    box.exec();
}

} // namespace

bool isEnabled()
{
    return MBCHESS_BETA_DAYS > 0;
}

QString downloadUrl()
{
    return QString::fromUtf8(MBCHESS_BETA_URL);
}

QDate expiryDate()
{
    if (!isEnabled())
        return QDate();
    const Record r = load();
    return r.firstStart.isValid() ? r.firstStart.addDays(MBCHESS_BETA_DAYS - 1) : QDate();
}

int daysLeft()
{
    if (!isEnabled())
        return -1;
    const Record r = load();
    if (!r.firstStart.isValid())
        return MBCHESS_BETA_DAYS;
    const qint64 left = today(r).daysTo(r.firstStart.addDays(MBCHESS_BETA_DAYS - 1)) + 1;
    return int(qMax<qint64>(0, left));
}

bool checkAtStartup(QWidget *parent)
{
    if (!isEnabled())
        return true;
    Record r = load();
    const bool first = !r.firstStart.isValid();
    const QDate now = today(r);
    if (first)
        r.firstStart = now;
    r.lastSeen = now;
    save(r);

    const QDate last = r.firstStart.addDays(MBCHESS_BETA_DAYS - 1);
    const qint64 left = now.daysTo(last) + 1;
    const QString title = QCoreApplication::translate("Beta", "mbChess test version");
    if (left <= 0) {
        message(parent, QMessageBox::Critical, title,
                QCoreApplication::translate("Beta", "<p><b>This test version of mbChess (%1) has expired.</b></p>"
                   "<p>It ran for %2 days, until %3. Please install the current build; your settings, "
                   "games and engines stay as they are.</p><p>%4</p>")
                    .arg(version())
                    .arg(MBCHESS_BETA_DAYS)
                    .arg(dateText(last), downloadSentence()));
        return false;
    }
    if (first) {
        message(parent, QMessageBox::Information, title,
                QCoreApplication::translate("Beta", "<p><b>Welcome to mbChess %1, a developer version for beta testers.</b></p>"
                   "<p>mbChess is still being built: it changes often, features may be missing or "
                   "unfinished, and there will be bugs. Please tell the developer what you find.</p>"
                   "<p>So that nobody keeps playing with an old build, this version runs for "
                   "%2 days from today, until %3. After that it only shows where the new one is.</p>"
                   "<p>%4</p>")
                    .arg(version())
                    .arg(MBCHESS_BETA_DAYS)
                    .arg(dateText(last), downloadSentence()));
    } else if (left <= kReminderDays) {
        message(parent, QMessageBox::Warning, title,
                (left == 1 ? QCoreApplication::translate("Beta", "<p><b>This test version of mbChess runs out today.</b></p>")
                           : QCoreApplication::translate("Beta", "<p><b>This test version of mbChess runs for %1 more days, until %2.</b></p>")
                                 .arg(left)
                                 .arg(dateText(last)))
                    + QCoreApplication::translate("Beta", "<p>Please install the current build soon.</p><p>%1</p>").arg(downloadSentence()));
    }
    return true;
}

QString aboutText()
{
    if (!isEnabled())
        return QString();
    const QDate last = expiryDate();
    if (!last.isValid())
        return QString();
    return QCoreApplication::translate("Beta", "Test version for beta testers: runs until %1.").arg(dateText(last));
}

} // namespace beta
