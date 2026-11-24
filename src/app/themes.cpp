// SPDX-License-Identifier: GPL-3.0-or-later
#include "themes.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace themes {

namespace {

QStringList subfolders(const QString &kind)
{
    QStringList names;
    for (const QString &root : searchFolders()) {
        const QDir dir(root + QLatin1Char('/') + kind);
        for (const QString &name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name | QDir::IgnoreCase))
            if (!names.contains(name, Qt::CaseInsensitive))
                names << name;
    }
    names.sort(Qt::CaseInsensitive);
    return names;
}

QString find(const QString &kind, const QString &name)
{
    if (name.isEmpty())
        return QString();
    for (const QString &root : searchFolders()) {
        const QString path = root + QLatin1Char('/') + kind + QLatin1Char('/') + name;
        if (QFileInfo(path).isDir())
            return path;
    }
    return QString();
}

ThemeInfo readInfo(const QString &id, const QString &folder)
{
    ThemeInfo info;
    info.id = id;
    info.name = id;
    info.folder = folder;
    QFile f(folder + QStringLiteral("/theme.ini"));
    if (!folder.isEmpty() && f.open(QIODevice::ReadOnly)) {
        // A tiny INI reader: QSettings would read Latin-1 under Qt 5.
        const QStringList lines = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n'));
        for (QString line : lines) {
            line = line.trimmed();
            const int eq = line.indexOf(QLatin1Char('='));
            if (line.isEmpty() || line.startsWith(QLatin1Char('[')) || line.startsWith(QLatin1Char(';')) || eq <= 0)
                continue;
            info.values.insert(line.left(eq).trimmed().toLower(), line.mid(eq + 1).trimmed());
        }
    }
    if (!info.values.value(QStringLiteral("name")).isEmpty())
        info.name = info.values.value(QStringLiteral("name"));
    info.group = info.values.value(QStringLiteral("group"));
    return info;
}

} // namespace

QColor ThemeInfo::color(const QString &key) const
{
    const QString v = values.value(key);
    return v.isEmpty() ? QColor() : QColor(v);
}

ThemeInfo boardInfo(const QString &id)
{
    return readInfo(id, boardFolder(id));
}

ThemeInfo pieceInfo(const QString &id)
{
    return readInfo(id, pieceFolder(id));
}

QStringList searchFolders()
{
    return { QStringLiteral(":/themes"), QCoreApplication::applicationDirPath() + QStringLiteral("/themes") };
}

QStringList boardThemes()
{
    QStringList l = subfolders(QStringLiteral("boards"));
    l.removeAll(defaultBoard());
    l.removeAll(classicBoard());
    l.prepend(classicBoard());
    l.prepend(defaultBoard());
    return l;
}

QStringList pieceSets()
{
    QStringList l = subfolders(QStringLiteral("pieces"));
    l.removeAll(defaultPieces());
    l.removeAll(classicPieces());
    l.prepend(classicPieces());
    l.prepend(defaultPieces());
    return l;
}

QStringList soundSets()
{
    return subfolders(QStringLiteral("sounds"));
}

QString boardFolder(const QString &name)
{
    return find(QStringLiteral("boards"), name);
}

QString pieceFolder(const QString &name)
{
    if (name == defaultPieces() || name == classicPieces())
        return QStringLiteral(":/pieces/cburnett");
    return find(QStringLiteral("pieces"), name);
}

QString soundFolder(const QString &name)
{
    return find(QStringLiteral("sounds"), name);
}

} // namespace themes
