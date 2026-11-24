# engine: start and control UCI engines (QProcess). No GUI code.
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = engine
CONFIG  += staticlib
QT       = core
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += uciengine.h
SOURCES += uciengine.cpp xboard.cpp
