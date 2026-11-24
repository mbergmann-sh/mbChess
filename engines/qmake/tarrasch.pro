# Tarrasch Toy Engine by Bill Forster (MIT), sources in tarrasch/src
#
# A small, weak UCI engine (the demo engine of the Tarrasch chess GUI): good as a
# beginner's opponent. Upstream is Windows-only; mbchess_port.h (marked "mbChess:")
# provides the few Windows calls on Linux and macOS.
include(../engine.pri)

TARGET = tarrasch
mbchessEngineDir(tarrasch)
CONFIG += c++17

TT = $$PWD/../tarrasch/src
SOURCES += $$TT/toyengine.cpp $$TT/ChessEngine.cpp $$TT/thc.cpp
HEADERS += $$TT/ChessEngine.h $$TT/thc.h $$TT/mbchess_port.h
INCLUDEPATH += $$TT
