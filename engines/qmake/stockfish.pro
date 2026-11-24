# Stockfish 19 (GPL-3.0-or-later), sources in stockfish/src
#
# The neural network (about 100 MB) is not part of the repository. It is
# downloaded once into the build folder by ../stockfish_net.pro, which is built
# first, and embedded into the program.
include(../engine.pri)

TARGET = stockfish
mbchessEngineDir(stockfish)
CONFIG -= object_parallel_to_source   # file names are unique

SF = $$PWD/../stockfish/src
SOURCES += \
    $$SF/attacks.cpp $$SF/benchmark.cpp $$SF/bitboard.cpp $$SF/evaluate.cpp $$SF/main.cpp \
    $$SF/misc.cpp $$SF/movegen.cpp $$SF/movepick.cpp $$SF/position.cpp \
    $$SF/search.cpp $$SF/thread.cpp $$SF/timeman.cpp $$SF/tt.cpp $$SF/uci.cpp \
    $$SF/ucioption.cpp $$SF/tune.cpp $$SF/syzygy/tbprobe.cpp \
    $$SF/nnue/nnue_accumulator.cpp $$SF/nnue/nnue_misc.cpp $$SF/nnue/network.cpp \
    $$SF/nnue/features/half_ka_v2_hm.cpp $$SF/nnue/features/full_threats.cpp \
    $$SF/nnue/features/pp_3wide.cpp \
    $$SF/engine.cpp $$SF/score.cpp $$SF/memory.cpp
HEADERS += $$files($$SF/*.h) $$files($$SF/nnue/*.h)

QMAKE_CXXFLAGS += -fno-exceptions -funroll-loops
DEFINES += NDEBUG ARCH=mbchess-$$MBCHESS_ENGINE_CPU

contains(QT_ARCH, .*64.*): DEFINES += IS_64BIT

equals(MBCHESS_ENGINE_CPU, avx2) {
    DEFINES += USE_AVX2 USE_SSE41 USE_SSSE3 USE_SSE2 USE_POPCNT
    QMAKE_CXXFLAGS += -msse -msse2 -msse3 -mssse3 -msse4.1 -mpopcnt -mavx2 -mbmi
} else: equals(MBCHESS_ENGINE_CPU, sse41) {
    DEFINES += USE_SSE41 USE_SSSE3 USE_SSE2 USE_POPCNT
    QMAKE_CXXFLAGS += -msse -msse2 -msse3 -mssse3 -msse4.1 -mpopcnt
} else: equals(MBCHESS_ENGINE_CPU, neon) {
    DEFINES += USE_NEON=8 USE_POPCNT
    macx: DEFINES += USE_NEON_DOTPROD
    macx: QMAKE_CXXFLAGS += -march=armv8.2-a+dotprod
} else {
    contains(QT_ARCH, x86_64) {
        DEFINES += USE_SSE2
        QMAKE_CXXFLAGS += -msse -msse2
    } else {
        DEFINES += NO_PREFETCH
    }
}

