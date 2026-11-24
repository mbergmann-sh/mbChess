# Texel by Peter Österlund (GPL-3.0-or-later), sources in texel/
#
# Built like upstream's CMake project (lib/texellib + app/texel), without the
# utilities. The neural network nndata.tbin.compr (6 MB) is part of the sources
# and embedded into the program with incbin. Texel reads Syzygy and Gaviota
# tablebases (options SyzygyPath, GaviotaTbPath).
include(../engine.pri)

TARGET = texel
mbchessEngineDir(texel)
CONFIG += c++17

TX = $$PWD/../texel/lib/texellib
SOURCES += \
    $$TX/book/book.cpp $$TX/book/polyglot.cpp \
    $$TX/debug/logger.cpp $$TX/debug/searchTreeSampler.cpp $$TX/debug/treeLogger.cpp \
    $$TX/hw/cluster.cpp $$TX/hw/clustertt.cpp $$TX/hw/largePageAlloc.cpp $$TX/hw/numa.cpp $$TX/hw/parallel.cpp \
    $$TX/nn/nneval.cpp $$TX/nn/nntypes.cpp \
    $$TX/tb/kpkTable.cpp $$TX/tb/krkpTable.cpp $$TX/tb/krpkrTable.cpp $$TX/tb/tbgen.cpp $$TX/tb/tbprobe.cpp \
    $$TX/tb/syzygy/rtb-probe.cpp \
    $$TX/util/random.cpp $$TX/util/timeUtil.cpp $$TX/util/util.cpp \
    $$TX/bitBoard.cpp $$TX/computerPlayer.cpp $$TX/endGameEval.cpp $$TX/evaluate.cpp $$TX/game.cpp \
    $$TX/history.cpp $$TX/humanPlayer.cpp $$TX/killerTable.cpp $$TX/material.cpp $$TX/move.cpp \
    $$TX/moveGen.cpp $$TX/parameters.cpp $$TX/piece.cpp $$TX/position.cpp $$TX/search.cpp \
    $$TX/textio.cpp $$TX/transpositionTable.cpp \
    $$PWD/../texel/app/texel/enginecontrol.cpp $$PWD/../texel/app/texel/texel.cpp \
    $$PWD/../texel/app/texel/tuigame.cpp $$PWD/../texel/app/texel/uciprotocol.cpp

# Gaviota tablebase probing code (C, MIT), as bundled with Texel
GTB = $$TX/tb/gtb
SOURCES += \
    $$GTB/compression/lzma/Lzma86Dec.c $$GTB/compression/lzma/LzFind.c $$GTB/compression/lzma/Lzma86Enc.c \
    $$GTB/compression/lzma/LzmaDec.c $$GTB/compression/lzma/Alloc.c $$GTB/compression/lzma/Bra86.c \
    $$GTB/compression/lzma/LzmaEnc.c $$GTB/compression/wrap.c $$GTB/gtb-dec.c $$GTB/gtb-att.c \
    $$GTB/sysport/sysport.c $$GTB/gtb-probe.c

INCLUDEPATH += $$TX $$TX/book $$TX/debug $$TX/hw $$TX/nn $$TX/tb $$TX/util \
    $$GTB/sysport $$GTB/compression $$GTB/compression/lzma

# The network, embedded with incbin: texel_nndata.cpp is written with the absolute path.
NNDATA = $$clean_path($$PWD/../texel/nndata.tbin.compr)
NNDATA_CPP = "$${LITERAL_HASH}include \"incbin.h\""
NNDATA_CPP += "INCBIN(NNData, \"$$NNDATA\");"
write_file($$OUT_PWD/texel_nndata.cpp, NNDATA_CPP)
SOURCES += $$OUT_PWD/texel_nndata.cpp

QMAKE_CXXFLAGS += -fno-stack-protector
unix:!macx: LIBS += -lrt -latomic
unix:!macx: DEFINES += HAS_RT

equals(MBCHESS_ENGINE_CPU, avx2) {
    DEFINES += USE_SSSE3 USE_AVX2 USE_BMI2 USE_POPCNT USE_CTZ USE_PREFETCH
    QMAKE_CXXFLAGS += -mssse3 -mavx2 -mbmi2 -mpopcnt
    QMAKE_CFLAGS += -mssse3 -mavx2 -mbmi2 -mpopcnt
} else: equals(MBCHESS_ENGINE_CPU, sse41) {
    DEFINES += USE_SSSE3 USE_POPCNT USE_CTZ USE_PREFETCH
    QMAKE_CXXFLAGS += -mssse3 -msse4.1 -mpopcnt
    QMAKE_CFLAGS += -mssse3 -msse4.1 -mpopcnt
} else: equals(MBCHESS_ENGINE_CPU, neon) {
    DEFINES += USE_NEON USE_CTZ USE_PREFETCH
} else {
    DEFINES += USE_CTZ
}
