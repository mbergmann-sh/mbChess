// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess voice recorder: records the words of the move announcements (and the
// greeting at start-up) as WAV files that mbChess uses as its own voice.
#include "recorderwindow.h"

#include <QApplication>
#include <QIcon>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    QApplication::setOrganizationName(QStringLiteral("mbChess"));
    QApplication::setApplicationName(QStringLiteral("VoiceRecorder"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(":/mbchess.png")));
    RecorderWindow w;
    w.show();
    return app.exec();
}
