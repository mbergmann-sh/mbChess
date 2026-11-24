# Toga II (multi-processor version) by Thomas Gaksch and others,
# based on Fruit (GPL-2.0-or-later), sources in toga/src
include(../engine.pri)

TARGET = toga
mbchessEngineDir(toga)

QMAKE_CXXFLAGS += -fno-exceptions -fno-rtti -fstrict-aliasing -fomit-frame-pointer

SOURCES += $$files($$PWD/../toga/src/*.cpp)
HEADERS += $$files($$PWD/../toga/src/*.h)
