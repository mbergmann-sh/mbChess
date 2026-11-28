// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - test builds for beta testers run for a limited time (MBCHESS_BETA_DAYS in
// common.pri, 42 days = 6 weeks; 0 switches it off). The time starts with the first start
// of this version (a new build starts its own six weeks). The first start shows a notice,
// the last week a reminder, after that mbChess only says where the new build is and quits.
//
// The date of the first start is kept in the settings and in a file in the user's data
// folder; the earlier one counts. If the clock is set back behind the last start, the
// last start counts as today.
#pragma once

#include <QDate>
#include <QString>

class QWidget;

namespace beta {

bool isEnabled();          // a time-limited build
QDate expiryDate();        // the last day the build runs; invalid if not enabled or never started
int daysLeft();            // including today; 0: expired
QString downloadUrl();     // MBCHESS_BETA_URL, may be empty

// Called at start before the main window: records the first start, shows the notice or
// the reminder. False: the build has expired, the program must quit.
bool checkAtStartup(QWidget *parent = nullptr);

// One line for the About dialog, empty if not enabled.
QString aboutText();

} // namespace beta
