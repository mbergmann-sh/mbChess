// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include <QString>

// Windows only: shows a .chm through the HTML Help API (hhctrl.ocx). Unlike opening the
// file with the shell, this keeps a single help window and brings it to the front again.
namespace htmlhelp {
bool show(const QString &chmFile); // false: HTML Help is not available
void closeAll();                   // closes the help window when mbChess quits
}
