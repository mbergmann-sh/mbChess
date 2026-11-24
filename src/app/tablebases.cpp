// SPDX-License-Identifier: GPL-3.0-or-later
#include "tablebases.h"

#include "datafolders.h"

#include <engine/uciengine.h>

#include <QDir>
#include <QFileInfo>
#include <QSettings>

namespace tablebases {

bool enabled()
{
    return QSettings().value(QStringLiteral("tablebases/use"), true).toBool();
}

void setEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("tablebases/use"), on);
}

QString folder()
{
    const QString f = QSettings().value(QStringLiteral("tablebases/folder")).toString();
    return f.isEmpty() ? datafolders::syzygyFolder() : f;
}

void setFolder(const QString &path)
{
    // the default is not stored, so that it follows the program folder
    QSettings s;
    if (QDir::cleanPath(path) == QDir::cleanPath(datafolders::syzygyFolder()))
        s.remove(QStringLiteral("tablebases/folder"));
    else
        s.setValue(QStringLiteral("tablebases/folder"), QDir::cleanPath(path));
}

QString activePath()
{
    if (!enabled())
        return QString();
    const QString f = folder();
    return status(f).wdl > 0 ? QDir::toNativeSeparators(QFileInfo(f).absoluteFilePath()) : QString();
}

QStringList tableNames(int maxPieces)
{
    // The same order as the Syzygy probing code (tbcore.c, init_tablebases).
    static const char pc[] = "KQRBNP";
    QStringList names;
    auto add = [&names](const QString &n) { names << n; };
    for (int i = 1; i < 6; ++i)
        add(QStringLiteral("K%1vK").arg(QLatin1Char(pc[i])));
    if (maxPieces >= 4) {
        for (int i = 1; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                add(QStringLiteral("K%1vK%2").arg(QLatin1Char(pc[i])).arg(QLatin1Char(pc[j])));
        for (int i = 1; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                add(QStringLiteral("K%1%2vK").arg(QLatin1Char(pc[i])).arg(QLatin1Char(pc[j])));
    }
    if (maxPieces >= 5) {
        for (int i = 1; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                for (int k = 1; k < 6; ++k)
                    add(QStringLiteral("K%1%2vK%3").arg(QLatin1Char(pc[i])).arg(QLatin1Char(pc[j])).arg(QLatin1Char(pc[k])));
        for (int i = 1; i < 6; ++i)
            for (int j = i; j < 6; ++j)
                for (int k = j; k < 6; ++k)
                    add(QStringLiteral("K%1%2%3vK").arg(QLatin1Char(pc[i])).arg(QLatin1Char(pc[j])).arg(QLatin1Char(pc[k])));
    }
    return names;
}

Status status(const QString &folderPath)
{
    Status st;
    const QDir dir(folderPath);
    const QStringList names = tableNames(5);
    st.expected = int(names.size());
    if (!dir.exists())
        return st;
    bool complete[6] = { true, true, true, true, true, true };
    for (const QString &n : names) {
        const int pieces = int(n.size()) - 1; // without the "v"
        const QFileInfo w(dir.filePath(n + QStringLiteral(".rtbw")));
        if (w.isFile() && w.size() > 0)
            ++st.wdl;
        else
            complete[pieces] = false;
        const QFileInfo z(dir.filePath(n + QStringLiteral(".rtbz")));
        if (z.isFile() && z.size() > 0)
            ++st.dtz;
    }
    for (int p = 3; p <= 5 && complete[p]; ++p)
        st.completeUpTo = p;
    return st;
}

bool isTablebaseOption(const QString &name)
{
    return name.compare(QLatin1String("SyzygyPath"), Qt::CaseInsensitive) == 0;
}

void applyToEngine(mbc::engine::UciEngine *engine, const QString &path)
{
    // Stockfish, Arasan and Viridithas use "SyzygyPath"; for Crafty the XBoard
    // layer offers the same name and turns it into "tbpath" + "egtb".
    // Without tables the engine keeps its default (no tables).
    if (!path.isEmpty() && engine->hasOption(QStringLiteral("SyzygyPath")))
        engine->setOption(QStringLiteral("SyzygyPath"), path);
}

} // namespace tablebases
