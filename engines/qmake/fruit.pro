# Fruit 2.1 by Fabien Letouzey (GPL-2.0-or-later), sources in fruit/src
include(../engine.pri)

TARGET = fruit
mbchessEngineDir(fruit)

QMAKE_CXXFLAGS += -fno-exceptions -fno-rtti -fstrict-aliasing -fomit-frame-pointer

SOURCES += $$files($$PWD/../fruit/src/*.cpp)
HEADERS += $$files($$PWD/../fruit/src/*.h)
