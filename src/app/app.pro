# mbChess application (Qt Widgets)
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = mbchess
QT      += core gui widgets svg network
DESTDIR  = $$MBCHESS_BIN_DIR

# order matters for static linking: users before their dependencies
qtHaveModule(serialport) {
    QT += serialport
    DEFINES += MBCHESS_HAVE_DGT
    mbchessLinkLib(dgt)
    HEADERS += dgtdialog.h
    SOURCES += dgtdialog.cpp
}
mbchessLinkLib(pgn)
mbchessLinkLib(book)
mbchessLinkLib(engine)
mbchessLinkLib(chesscore)

HEADERS += \
    benchmark.h \
    benchmarkdialog.h \
    benchmarkwindow.h \
    boardwidget.h \
    datafolders.h \
    chessclock.h \
    enginedialog.h \
    enginemanager.h \
    evalgraphwidget.h \
    gamelistwidget.h \
    gamemanager.h \
    guitheme.h \
    soundplayer.h \
    tablebasedialog.h \
    engineanalysis.h \
    boardeditordialog.h \
    matesearchdialog.h \
    splashscreen.h \
    statusfield.h \
    tablebases.h \
    themes.h \
    notationwidget.h \
    newgamedialog.h \
    mainwindow.h \
    languagemanager.h \
    ../libs/compat/compat.h

SOURCES += \
    benchmark.cpp \
    benchmarkdialog.cpp \
    benchmarkwindow.cpp \
    boardwidget.cpp \
    chessclock.cpp \
    enginedialog.cpp \
    enginemanager.cpp \
    evalgraphwidget.cpp \
    gamelistwidget.cpp \
    gamemanager.cpp \
    guitheme.cpp \
    soundplayer.cpp \
    tablebasedialog.cpp \
    engineanalysis.cpp \
    boardeditordialog.cpp \
    matesearchdialog.cpp \
    splashscreen.cpp \
    statusfield.cpp \
    tablebases.cpp \
    themes.cpp \
    notationwidget.cpp \
    newgamedialog.cpp \
    main.cpp \
    mainwindow.cpp \
    languagemanager.cpp

# The themes are large: let rcc write an object file directly instead of a huge C++ file.
CONFIG += resources_big
RESOURCES += resources/resources.qrc resources/themes.qrc

# Move sounds and announcements need Qt Multimedia. Without it mbChess builds and
# runs silently (Qt Maintenance Tool: add "Qt Multimedia" to the kit to get sound).
qtHaveModule(multimedia) {
    QT += multimedia
    DEFINES += MBCHESS_HAVE_SOUND
} else {
    message("mbChess: Qt Multimedia nicht gefunden - ohne Klaenge / no Qt Multimedia - no sound")
}

# --- Translations (German / English), compiled into the program ---
TRANSLATIONS += \
    ../../translations/mbchess_de.ts \
    ../../translations/mbchess_en.ts
CONFIG += lrelease embed_translations

# Qt's own German texts (Yes/No/Cancel of the standard dialogs) are embedded too, so that they
# work wherever mbChess is started from (windeployqt only writes a combined qt_de.qm).
QT_QM_DIR = $$[QT_INSTALL_TRANSLATIONS]
exists($$QT_QM_DIR/qtbase_de.qm) {
    QT_QRC = "<RCC><qresource prefix=\"/i18n\">"
    QT_QRC += "<file alias=\"qtbase_de.qm\">$$QT_QM_DIR/qtbase_de.qm</file>"
    QT_QRC += "</qresource></RCC>"
    write_file($$OUT_PWD/qt_translations.qrc, QT_QRC)
    RESOURCES += $$OUT_PWD/qt_translations.qrc
} else {
    message("mbChess: $$QT_QM_DIR/qtbase_de.qm fehlt - Standard-Knoepfe bleiben englisch / standard buttons stay English")
}

# Version name ("codename") from common.pri, as a generated header (it has spaces)
VERSION_HEADER = "$${LITERAL_HASH}define MBCHESS_CODENAME \"$$MBCHESS_CODENAME\""
write_file($$OUT_PWD/mbchess_codename.h, VERSION_HEADER)
INCLUDEPATH += $$OUT_PWD

# --- Platform specifics ---
win32 {
    HEADERS += htmlhelp.h
    SOURCES += htmlhelp_win.cpp
    VERSION = $$MBCHESS_VERSION
    QMAKE_TARGET_PRODUCT     = mbChess
    QMAKE_TARGET_DESCRIPTION = "mbChess chess GUI"
    QMAKE_TARGET_COPYRIGHT   = "(C) Michael Bergmann 2026, GPL-3.0-or-later"
    RC_ICONS = resources/icons/mbchess.ico
}
macx {
    TARGET = mbChess
    ICON   = resources/icons/mbchess.icns
}
# android { }  -> phase 6 (ANDROID_PACKAGE_SOURCE_DIR, engines as ANDROID_EXTRA_LIBS)

unix:!macx:!android {
    target.path = /usr/bin
    desktop.files = ../../dist/linux/mbchess.desktop
    desktop.path  = /usr/share/applications
    icon.files    = resources/icons/mbchess.svg
    icon.path     = /usr/share/icons/hicolor/scalable/apps
    INSTALLS += target desktop icon
}

# Strings of the libraries are translated together with the program.
lupdate_only {
    SOURCES += ../libs/engine/uciengine.cpp ../libs/engine/xboard.cpp ../libs/dgt/dgtboard.cpp
}

# The manual (docs/manual) next to the program, as the Windows installer puts it:
# Handbuch/mbChess-Handbuch.pdf (German), Manual/mbChess-Manual.pdf (English).
# Help > Manual (F1) opens it; on Windows a .chm with the same name comes first.
# The help files (.chm, built by tools/build-manual.py) are copied on Windows only.
exists($$MBCHESS_ROOT/docs/manual/mbChess-Handbuch.pdf) {
    manual_de.files = $$MBCHESS_ROOT/docs/manual/mbChess-Handbuch.pdf
    win32:exists($$MBCHESS_ROOT/docs/manual/mbChess-Handbuch.chm): \
        manual_de.files += $$MBCHESS_ROOT/docs/manual/mbChess-Handbuch.chm
    manual_de.path  = $$MBCHESS_BIN_DIR/Handbuch
    COPIES += manual_de
}
exists($$MBCHESS_ROOT/docs/manual/mbChess-Manual.pdf) {
    manual_en.files = $$MBCHESS_ROOT/docs/manual/mbChess-Manual.pdf
    win32:exists($$MBCHESS_ROOT/docs/manual/mbChess-Manual.chm): \
        manual_en.files += $$MBCHESS_ROOT/docs/manual/mbChess-Manual.chm
    manual_en.path  = $$MBCHESS_BIN_DIR/Manual
    COPIES += manual_en
}
