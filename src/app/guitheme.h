// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - look of the program window (not the board): the system's own style, or
// the dark design from the editor AmigaED 4 (same author, GPL).
#pragma once

#include <QIcon>
#include <QString>
#include <QStringList>

namespace guitheme {

QStringList ids();                 // "system", "dark"
inline QString defaultId() { return QStringLiteral("system"); }

// Applies a theme to the whole application (unknown ids: the system style).
void apply(const QString &id);
QString current();
bool isDark();                     // true if the window colour is dark

// A toolbar icon from :/icons/toolbar/<name>.svg, recoloured for dark themes.
QIcon toolbarIcon(const QString &name);

} // namespace guitheme
