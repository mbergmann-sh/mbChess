// SPDX-License-Identifier: GPL-3.0-or-later
#include "enginemanager.h"

#include <engine/uciengine.h>

#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace enginemanager {

namespace {

QString key(const EngineEntry &e)
{
    const QString raw = e.inFolder ? QStringLiteral("folder:") + QFileInfo(e.path).fileName().toLower()
                                   : QStringLiteral("path:") + QDir::cleanPath(e.path);
    return QStringLiteral("engines/") + QString::fromLatin1(QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Md5).toHex().left(12));
}

bool isInFolder(const QString &path)
{
    const QString folder = QDir::cleanPath(mbc::engine::enginesDirectory()) + QLatin1Char('/');
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).startsWith(folder);
}

void loadUserData(EngineEntry *e)
{
    QSettings s;
    s.beginGroup(key(*e));
    e->name = s.value(QStringLiteral("name"), e->defaultName).toString();
    if (e->name.trimmed().isEmpty())
        e->name = e->defaultName;
    s.beginGroup(QStringLiteral("options"));
    for (const QString &k : s.childKeys())
        e->options.insert(QString::fromUtf8(QByteArray::fromPercentEncoding(k.toLatin1())), s.value(k).toString());
}

QStringList userPaths()
{
    return QSettings().value(QStringLiteral("engines/user")).toStringList();
}

} // namespace

EngineEntry entry(const QString &path)
{
    EngineEntry e;
    if (path.isEmpty())
        return e;
    const mbc::engine::EngineInfo info = mbc::engine::engineInfo(QFileInfo(path).absoluteFilePath());
    e.defaultName = info.name;
    e.path = info.path.isEmpty() ? QFileInfo(path).absoluteFilePath() : info.path;
    e.arguments = info.arguments;
    e.protocol = info.protocol;
    e.inFolder = isInFolder(e.path);
    loadUserData(&e);
    return e;
}

QVector<EngineEntry> engines()
{
    QVector<EngineEntry> list;
    QVector<mbc::engine::EngineInfo> found = mbc::engine::findEngines();
    std::stable_sort(found.begin(), found.end(), [](const auto &a, const auto &b) {
        return (a.name == QLatin1String("Stockfish")) > (b.name == QLatin1String("Stockfish"));
    });
    for (const auto &info : found)
        list.append(entry(info.path));
    for (const QString &p : userPaths()) {
        const bool known = std::any_of(list.cbegin(), list.cend(), [&](const EngineEntry &e) {
            return QFileInfo(e.path) == QFileInfo(p);
        });
        if (!known)
            list.append(entry(p));
    }
    return list;
}

bool add(const QString &path)
{
    const QString abs = QFileInfo(path).absoluteFilePath();
    for (const EngineEntry &e : engines())
        if (QFileInfo(e.path) == QFileInfo(abs))
            return false;
    QStringList l = userPaths();
    l.append(abs);
    QSettings().setValue(QStringLiteral("engines/user"), l);
    return true;
}

void remove(const QString &path)
{
    QStringList l = userPaths();
    l.erase(std::remove_if(l.begin(), l.end(), [&](const QString &p) { return QFileInfo(p) == QFileInfo(path); }), l.end());
    QSettings s;
    s.setValue(QStringLiteral("engines/user"), l);
    s.remove(key(entry(path)));
}

void rename(const QString &path, const QString &name)
{
    const EngineEntry e = entry(path);
    QSettings s;
    if (name.trimmed().isEmpty() || name.trimmed() == e.defaultName)
        s.remove(key(e) + QStringLiteral("/name"));
    else
        s.setValue(key(e) + QStringLiteral("/name"), name.trimmed());
}

void setOptions(const QString &path, const QMap<QString, QString> &options)
{
    const EngineEntry e = entry(path);
    QSettings s;
    s.remove(key(e) + QStringLiteral("/options"));
    s.beginGroup(key(e) + QStringLiteral("/options"));
    // option names may contain spaces and other characters QSettings does not like
    for (auto it = options.cbegin(); it != options.cend(); ++it)
        s.setValue(QString::fromLatin1(it.key().toUtf8().toPercentEncoding()), it.value());
}

bool isManagedOption(const QString &name)
{
    static const QStringList managed = {
        QStringLiteral("Hash"), QStringLiteral("Threads"), QStringLiteral("Cores"),
        QStringLiteral("UCI_LimitStrength"), QStringLiteral("UCI_Elo"), QStringLiteral("Skill Level"), QStringLiteral("Skill_Level"),
        QStringLiteral("OwnBook"), QStringLiteral("BookFile"), QStringLiteral("Book File"), QStringLiteral("BookPath"),
        QStringLiteral("Ponder"), QStringLiteral("MultiPV"), QStringLiteral("UCI_AnalyseMode"),
        QStringLiteral("UCI_Chess960"), QStringLiteral("UCI_ShowWDL"), QStringLiteral("SyzygyPath"),
        QStringLiteral("GaviotaTbPath"), QStringLiteral("GaviotaTbCache"),
    };
    return managed.contains(name, Qt::CaseInsensitive);
}

int patriciaSkillLevel(int elo)
{
    // the table in Patricia's uci.h
    static const int levels[20] = { 500,  800,  1000, 1200, 1300, 1400, 1500, 1600, 1700, 1800,
                                    1900, 2000, 2100, 2200, 2300, 2400, 2500, 2650, 2800, 3000 };
    if (elo <= 0)
        return 21;
    int best = 0;
    for (int i = 1; i < 20; ++i)
        if (qAbs(levels[i] - elo) < qAbs(levels[best] - elo))
            best = i;
    return best + 1;
}

} // namespace enginemanager
