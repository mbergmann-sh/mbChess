# mbChess - Schach-GUI / chess GUI
# Copyright (C) 2026 Michael Bergmann
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Top-level project: build everything with
#   qmake mbChess.pro && make          (Linux/macOS)
#   qmake mbChess.pro && mingw32-make  (Windows, MinGW)
# Run the unit tests with "make check".

TEMPLATE = subdirs

SUBDIRS += \
    chesscore \
    pgn \
    book \
    engine \
    app \
    tests

chesscore.subdir = src/libs/chesscore
pgn.subdir       = src/libs/pgn
pgn.depends      = chesscore
book.subdir      = src/libs/book
book.depends     = chesscore
engine.subdir    = src/libs/engine
engine.depends   = chesscore

app.subdir  = src/app
app.depends = chesscore pgn book engine

# DGT e-boards need Qt Serial Port (Qt Maintenance Tool: "Qt Serial Port").
# Without it mbChess builds without DGT support.
qtHaveModule(serialport) {
    SUBDIRS += dgt
    dgt.subdir  = src/libs/dgt
    dgt.depends = chesscore
    app.depends += dgt
} else {
    message("mbChess: Qt Serial Port nicht gefunden - ohne DGT-Brett / no Qt Serial Port - no DGT board")
}

tests.subdir  = tests
tests.depends = chesscore pgn book engine
qtHaveModule(serialport): tests.depends += dgt

# Free UCI engines, built from engines/ into bin/engines/ (see engines/README.md).
# Leave them out with:  qmake CONFIG+=no_engines
!no_engines {
    SUBDIRS += engines
    engines.subdir = engines
}

# Voice recorder (tools/voicerecorder, Qt 6 + Multimedia), built next to the program
# only if it is not there yet. Leave it out with:  qmake CONFIG+=no_voicerecorder
!no_voicerecorder {
    SUBDIRS += voicerecorder
    voicerecorder.file = tools/voicerecorder-build.pro
}

# Windows installers (German and English, Qt Installer Framework) as the last
# step:  qmake CONFIG+=installer   or run  install\make-installer.bat <build folder>
installer {
    SUBDIRS += installer
    installer.subdir  = install
    installer.depends = app
    !no_engines: installer.depends += engines
    !no_voicerecorder: installer.depends += voicerecorder
}

# Later phases add: net, training (see docs/ARCHITEKTUR.md)

OTHER_FILES += \
    README.md \
    common.pri \
    docs/ARCHITEKTUR.md \
    .github/workflows/build.yml
