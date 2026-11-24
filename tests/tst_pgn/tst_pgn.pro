# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = tst_pgn
QT       = core testlib
CONFIG  += testcase console
CONFIG  -= app_bundle

mbchessLinkLib(pgn)
mbchessLinkLib(chesscore)

SOURCES += tst_pgn.cpp
