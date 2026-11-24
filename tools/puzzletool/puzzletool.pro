# Developer tool: checks mate puzzle collections with the exact mate solver of chesscore
# (tools/make-mate-puzzles.py uses it). Not part of the normal build:
#   qmake tools/puzzletool/puzzletool.pro && make
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../common.pri)

TEMPLATE = app
TARGET   = mbchess-puzzletool
QT       = core
CONFIG  += console
CONFIG  -= app_bundle

mbchessLinkLib(chesscore)

SOURCES += main.cpp
