# Glaurung 2.2 by Tord Romstad (GPL-3.0-or-later), sources in glaurung/src
include(../engine.pri)

TARGET = glaurung
mbchessEngineDir(glaurung)

DEFINES += NDEBUG
QMAKE_CXXFLAGS += -fno-exceptions -fno-rtti -fstrict-aliasing -fomit-frame-pointer

SOURCES += $$files($$PWD/../glaurung/src/*.cpp)
# timeoday.cpp is a gettimeofday() for MSVC only; MinGW, Linux and macOS have their own
SOURCES -= $$PWD/../glaurung/src/timeoday.cpp
HEADERS += $$files($$PWD/../glaurung/src/*.h)
