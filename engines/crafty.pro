# Crafty 25.2 by Robert Hyatt (XBoard protocol), built from its upstream sources.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Crafty's licence allows personal use only and forbids redistribution. That is why
# its source is NOT part of mbChess: "make" downloads it once into the build folder
#   <build>/engines-src/crafty   (git clone, or the GitHub archive with curl + tar)
# and compiles it into <program folder>/engines/crafty/, together with its opening
# book book.bin. The Windows installer leaves this folder out.
# Skip it with  qmake "MBCHESS_ENGINES=stockfish arasan ..."  (without crafty).
include(engine.pri)

TEMPLATE = aux
QMAKE_EXTRA_TARGETS += first

CRAFTY_GIT = https://github.com/MartinMSPedersen/Crafty-Chess.git
CRAFTY_TAR = https://github.com/MartinMSPedersen/Crafty-Chess/archive/refs/heads/master.tar.gz

SRC_ROOT = $$MBCHESS_BUILD_ROOT/engines-src
SRC_DIR  = $$SRC_ROOT/crafty
SRC_MAIN = $$SRC_DIR/source/crafty.c
OUT_DIR  = $$MBCHESS_ENGINE_DIR/crafty
win32: OUT_EXE = $$OUT_DIR/crafty.exe
else:  OUT_EXE = $$OUT_DIR/crafty
OUT_BOOK = $$OUT_DIR/book.bin

q = $$shell_quote($$shell_path($$SRC_DIR))
qpart = $$shell_quote($$shell_path($${SRC_DIR}.part))
qtar = $$shell_quote($$shell_path($$SRC_ROOT/crafty.tar.gz))
qroot = $$shell_quote($$shell_path($$SRC_ROOT))

# 1. Source: once. git first, then the archive (curl and tar are part of Windows 10/11).
win32 {
    clean_part = if exist $$qpart rmdir /s /q $$qpart
    tar_cmd = ( curl -L --fail --retry 2 -o $$qtar $$shell_quote($$CRAFTY_TAR) && mkdir $$qpart && tar -xzf $$qtar -C $$qpart --strip-components=1 )
} else {
    clean_part = rm -rf $$qpart
    tar_cmd = ( curl -L --fail --retry 2 -o $$qtar $$shell_quote($$CRAFTY_TAR) && mkdir -p $$qpart && tar -xzf $$qtar -C $$qpart --strip-components=1 )
}
crafty_src.target   = $$SRC_MAIN
crafty_src.commands = ( $$sprintf($$QMAKE_MKDIR_CMD, $$qroot) ) && ( $$clean_part ) && \
    ( git clone --depth 1 $$shell_quote($$CRAFTY_GIT) $$qpart || ( ( $$clean_part ) && $$tar_cmd ) ) && \
    $(MOVE) $$qpart $$q

# 2. Compile: Crafty is one translation unit (crafty.c includes all other files).
CRAFTY_FLAGS = -O3 -w -fomit-frame-pointer -DSYZYGY -DSKILL -DCPUS=64
unix: CRAFTY_FLAGS += -DUNIX
equals(MBCHESS_ENGINE_CPU, avx2):  CRAFTY_FLAGS += -mpopcnt -mavx2
equals(MBCHESS_ENGINE_CPU, sse41): CRAFTY_FLAGS += -mpopcnt -msse4.1
CRAFTY_LIBS = -pthread -lm
win32-g++: CRAFTY_LIBS += -static -static-libgcc

crafty_exe.target   = $$OUT_EXE
crafty_exe.depends  = $$SRC_MAIN
crafty_exe.commands = ( $$sprintf($$QMAKE_MKDIR_CMD, $$shell_quote($$shell_path($$OUT_DIR))) ) && \
    $$QMAKE_CC $$CRAFTY_FLAGS -o $$shell_quote($$shell_path($$OUT_EXE)) \
        $$shell_quote($$shell_path($$SRC_MAIN)) $$CRAFTY_LIBS

# 3. Opening book next to the program (Crafty reads book.bin from its working folder).
crafty_book.target   = $$OUT_BOOK
crafty_book.depends  = $$SRC_MAIN
crafty_book.commands = ( $$sprintf($$QMAKE_MKDIR_CMD, $$shell_quote($$shell_path($$OUT_DIR))) ) && \
    $(COPY_FILE) $$shell_quote($$shell_path($$SRC_DIR/books/book.bin)) $$shell_quote($$shell_path($$OUT_BOOK))

first.depends += $$OUT_EXE $$OUT_BOOK
QMAKE_EXTRA_TARGETS += crafty_src crafty_exe crafty_book
