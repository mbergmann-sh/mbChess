# book: PolyGlot opening books and opening names. No GUI code.
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = book
CONFIG  += staticlib
QT       = core
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += polyglot.h openings.h bookbuilder.h
SOURCES += polyglot.cpp openings.cpp bookbuilder.cpp
OTHER_FILES += polyglotkeys.inc
