# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = tst_engine
QT       = core testlib
CONFIG  += testcase console
CONFIG  -= app_bundle

mbchessLinkLib(engine)
mbchessLinkLib(chesscore)

SOURCES += tst_engine.cpp
