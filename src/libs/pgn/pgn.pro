# pgn: read and write PGN files. No GUI code.
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = pgn
CONFIG  += staticlib
QT       = core
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += pgn.h pgnindex.h
SOURCES += pgn.cpp pgnindex.cpp
