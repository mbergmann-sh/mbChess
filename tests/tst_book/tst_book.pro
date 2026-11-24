# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = tst_book
QT       = core testlib
CONFIG  += testcase console
CONFIG  -= app_bundle

mbchessLinkLib(book)
mbchessLinkLib(chesscore)

SOURCES += tst_book.cpp
