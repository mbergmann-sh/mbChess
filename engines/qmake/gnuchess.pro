# GNU Chess 6.2.5 (GPL-3.0-or-later), sources in gnuchess/src
#
# Built without "configure": gnuchess-config/config.h replaces the generated one.
# Linux and macOS only (GNU Chess needs POSIX pipes and select()).
# mbChess starts it with "--uci".
include(../engine.pri)

TARGET = gnuchess
mbchessEngineDir(gnuchess)

GC = $$PWD/../gnuchess/src
SOURCES += \
    $$GC/main.cc $$GC/components.cc \
    $$files($$GC/frontend/*.cc) \
    $$files($$GC/adapter/*.cpp) \
    $$files($$GC/engine/*.cpp)
HEADERS += $$files($$GC/*.h) $$files($$GC/frontend/*.h) $$files($$GC/adapter/*.h) $$files($$GC/engine/*.h)

INCLUDEPATH += $$PWD/../gnuchess-config $$PWD/../gnuchess/lib $$GC
DEFINES += HAVE_CONFIG_H NDEBUG
QMAKE_CXXFLAGS += -pthread
LIBS += -pthread

# Opening book and settings, next to the program
gc_data.files = $$GC/smallbook.bin $$GC/gnuchess.ini
gc_data.path  = $$DESTDIR
CONFIG += file_copies
COPIES += gc_data
