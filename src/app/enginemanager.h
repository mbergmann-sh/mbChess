// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - engine management: the engines in the program's "engines" folder plus
// engines the user added, each with a display name and its own UCI options.
//
// Settings: engines/user (list of executable paths), engines/<key>/name and
// engines/<key>/options/<option name> (only values that differ from the default).
// The key of a folder engine is its file name, so the settings survive a move of
// the program folder; added engines are keyed by their path.
#pragma once

#include <engine/uciengine.h>

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVector>

struct EngineEntry
{
    QString name;          // shown in the program (may be renamed)
    QString defaultName;   // as detected
    QString path;          // absolute path of the executable
    QStringList arguments; // command line for UCI (or XBoard) mode
    mbc::engine::Protocol protocol = mbc::engine::Protocol::Uci;
    bool inFolder = false; // from the "engines" folder (cannot be removed)
    QMap<QString, QString> options; // UCI options set by the user

    bool isValid() const { return !path.isEmpty(); }
};

namespace enginemanager {

// All engines: folder engines first (Stockfish on top), then added ones.
QVector<EngineEntry> engines();
// The entry for an executable (also one that is not in the list yet).
EngineEntry entry(const QString &path);

// Adds an engine chosen by the user; returns false if it is already in the list.
bool add(const QString &path);
void remove(const QString &path);
void rename(const QString &path, const QString &name);
void setOptions(const QString &path, const QMap<QString, QString> &options);

// Options that mbChess sets itself (strength, cores, hash, books, ...) and that are
// therefore not offered in the options dialog.
bool isManagedOption(const QString &name);

} // namespace enginemanager
