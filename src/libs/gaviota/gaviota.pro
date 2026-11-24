# gaviota: Gaviota endgame tablebases (3-4-5 pieces, distance to mate), probing code
# by Miguel A. Ballicora (MIT, sources in gtb/, upstream michiguel/Gaviota-Tablebases)
# and a small C++ layer for mbChess positions (gaviotatb.h).
# SPDX-License-Identifier: GPL-3.0-or-later

include(../../../common.pri)

TEMPLATE = lib
TARGET   = gaviota
CONFIG  += staticlib warn_off
CONFIG  -= warn_on
QT       = core
DESTDIR  = $$MBCHESS_LIB_DIR

HEADERS += gaviotatb.h
SOURCES += gaviotatb.cpp

GTB = $$PWD/gtb
# as in upstream's Makefile: zlib with prefixed names (no clash with a system zlib)
DEFINES += Z_PREFIX NDEBUG
# MinGW: Windows threads and file functions instead of the POSIX ones (as upstream's makefile.mingw)
win32-g++: DEFINES += MINGW
INCLUDEPATH += $$GTB $$GTB/sysport $$GTB/compression $$GTB/compression/liblzf \
    $$GTB/compression/zlib $$GTB/compression/lzma $$GTB/compression/huffman

SOURCES += \
    $$GTB/gtb-probe.c $$GTB/gtb-dec.c $$GTB/gtb-att.c \
    $$GTB/sysport/sysport.c \
    $$GTB/compression/wrap.c \
    $$GTB/compression/huffman/hzip.c \
    $$GTB/compression/lzma/LzmaEnc.c $$GTB/compression/lzma/LzmaDec.c $$GTB/compression/lzma/Alloc.c \
    $$GTB/compression/lzma/LzFind.c $$GTB/compression/lzma/Lzma86Enc.c $$GTB/compression/lzma/Lzma86Dec.c \
    $$GTB/compression/lzma/Bra86.c \
    $$GTB/compression/zlib/zcompress.c $$GTB/compression/zlib/uncompr.c $$GTB/compression/zlib/inflate.c \
    $$GTB/compression/zlib/deflate.c $$GTB/compression/zlib/adler32.c $$GTB/compression/zlib/crc32.c \
    $$GTB/compression/zlib/infback.c $$GTB/compression/zlib/inffast.c $$GTB/compression/zlib/inftrees.c \
    $$GTB/compression/zlib/trees.c $$GTB/compression/zlib/zutil.c \
    $$GTB/compression/liblzf/lzf_c.c $$GTB/compression/liblzf/lzf_d.c

OTHER_FILES += readme.txt license.txt
