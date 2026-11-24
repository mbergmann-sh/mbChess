# chesscore: rules, move generation, FEN, SAN. No GUI code.
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = chesscore
CONFIG  += staticlib
QT       = core
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += \
    types.h \
    move.h \
    attacks.h \
    position.h \
    notation.h \
    game.h

SOURCES += \
    attacks.cpp \
    position.cpp \
    notation.cpp \
    game.cpp
