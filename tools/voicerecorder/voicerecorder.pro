# mbChess voice recorder (Qt 6): record your own move announcements.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# A separate project, not part of mbChess.pro: open it in Qt Creator on its own.
# Put the program next to mbchess.exe (or choose the folder themes/sounds of
# mbChess in the program); the recorded voice then appears in mbChess under
# Board > Voice.

lessThan(QT_MAJOR_VERSION, 6): error("The voice recorder needs Qt 6 / Die Sprachaufnahme braucht Qt 6")
!qtHaveModule(multimedia): error("Qt Multimedia is missing / Qt Multimedia fehlt")

TEMPLATE = app
TARGET   = mbchess-voicerecorder
QT      += widgets multimedia
CONFIG  += c++17 warn_on
CONFIG  -= debug_and_release

HEADERS += recorderwindow.h wav.h words.h
SOURCES += main.cpp recorderwindow.cpp wav.cpp
RESOURCES += voicerecorder.qrc

win32 {
    RC_ICONS = ../../src/app/resources/icons/mbchess.ico
    QMAKE_TARGET_DESCRIPTION = "mbChess voice recorder"
    QMAKE_TARGET_COPYRIGHT   = "(C) Michael Bergmann 2026, GPL-3.0-or-later"
}
