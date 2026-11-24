# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = tst_dgt
QT       = core serialport testlib
CONFIG  += testcase console
CONFIG  -= app_bundle

mbchessLinkLib(dgt)
mbchessLinkLib(chesscore)

SOURCES += tst_dgt.cpp
