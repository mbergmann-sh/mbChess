# Patricia by Adam Kulju (MIT), sources in patricia/engine
#
# A very aggressive engine (C++20). Its three small networks (nets/, 1.5 MB each)
# are embedded with incbin; Fathom (MIT) probes Syzygy tablebases.
include(../engine.pri)

TARGET = patricia
mbchessEngineDir(patricia)
CONFIG -= c++11 c++14 c++17
CONFIG += c++2a
QMAKE_CXXFLAGS += -std=c++20

PA = $$PWD/../patricia/engine
SOURCES += $$PA/src/patricia.cpp $$PA/src/fathom/src/tbprobe.c
HEADERS += $$files($$PA/src/*.h)
INCLUDEPATH += $$PA/src
# incbin: the assembler looks for "nets/..." relative to the engine folder
QMAKE_CXXFLAGS += -Wa,-I$$shell_quote($$PA)
QMAKE_CXXFLAGS += -ffast-math
unix:!macx: LIBS += -lm

equals(MBCHESS_ENGINE_CPU, avx2) {
    QMAKE_CXXFLAGS += -mavx2 -mbmi -mbmi2 -mpopcnt -mfma
} else: equals(MBCHESS_ENGINE_CPU, sse41) {
    QMAKE_CXXFLAGS += -msse4.1 -mpopcnt
}
