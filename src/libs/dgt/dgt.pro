# dgt: DGT electronic chess boards (USB, Bluetooth Classic, RS232 = serial port).
# Needs Qt Serial Port. No GUI code.
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = dgt
CONFIG  += staticlib
QT       = core serialport
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += \
    dgtprotocol.h \
    dgtboard.h \
    dgtscanner.h

SOURCES += \
    dgtprotocol.cpp \
    dgtboard.cpp \
    dgtscanner.cpp
