# Arasan 26.1 by Jon Dart (MIT licence), sources in arasan/src
#
# Arasan loads its neural network at run time from its own folder. The network
# (about 25 MB) is downloaded once and placed next to the program.
include(../engine.pri)

TARGET = arasan
mbchessEngineDir(arasan)
CONFIG -= object_parallel_to_source   # file names are unique

AR = $$PWD/../arasan/src
SOURCES += \
    $$AR/arasanx.cpp $$AR/tester.cpp $$AR/bench.cpp $$AR/protocol.cpp \
    $$AR/input.cpp $$AR/globals.cpp $$AR/board.cpp $$AR/boardio.cpp $$AR/material.cpp \
    $$AR/chess.cpp $$AR/attacks.cpp \
    $$AR/bitutil.cpp $$AR/chessio.cpp $$AR/epdrec.cpp $$AR/bhash.cpp \
    $$AR/scoring.cpp $$AR/see.cpp \
    $$AR/movearr.cpp $$AR/notation.cpp $$AR/options.cpp $$AR/bitprobe.cpp \
    $$AR/bookread.cpp $$AR/bookwrit.cpp \
    $$AR/search.cpp $$AR/searchc.cpp $$AR/learn.cpp \
    $$AR/movegen.cpp $$AR/hash.cpp $$AR/calctime.cpp $$AR/eco.cpp $$AR/legal.cpp \
    $$AR/stats.cpp $$AR/threadp.cpp $$AR/threadc.cpp $$AR/evaluate.cpp \
    $$AR/syzygy.cpp $$PWD/arasan_tbprobe.cpp   # Syzygy tablebases (Fathom, MIT)
HEADERS += $$files($$AR/*.h)
INCLUDEPATH += $$AR $$AR/nnue

AR_NET = arasanv8-20261005.nnue
DEFINES += NDEBUG USE_INTRINSICS USE_ASM SMP_STATS SYZYGY_TBS NETWORK=$$AR_NET ARASAN_VERSION=26.1
QMAKE_CXXFLAGS += -fno-strict-aliasing -fno-rtti

contains(QT_ARCH, .*64.*): DEFINES += _64BIT

equals(MBCHESS_ENGINE_CPU, avx2) {
    DEFINES += SIMD SSE2 USE_POPCNT SSSE3 SSE41 AVX2
    QMAKE_CXXFLAGS += -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mpopcnt -mavx2 -mfma
} else: equals(MBCHESS_ENGINE_CPU, sse41) {
    DEFINES += SIMD SSE2 USE_POPCNT SSSE3 SSE41
    QMAKE_CXXFLAGS += -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mpopcnt
} else: equals(MBCHESS_ENGINE_CPU, neon) {
    DEFINES += SIMD NEON USE_POPCNT
} else: contains(QT_ARCH, x86_64) {
    DEFINES += SIMD SSE2
    QMAKE_CXXFLAGS += -msse2 -msse3
}

mbchessDownload(arasan_net, $$DESTDIR/$$AR_NET, \
    https://raw.githubusercontent.com/jdart1/arasan-chess/v26.1/network/$$AR_NET)
