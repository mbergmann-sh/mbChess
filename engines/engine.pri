# Shared settings for building a UCI engine from its upstream sources.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Each engine is a plain console program, compiled with the same compiler as
# mbChess (MinGW on Windows, gcc/clang on Linux and macOS), and placed in
#   <program folder>/engines/<engine>/
# "make" only compiles what is missing or changed; finished engines are left alone.
# Network files that are too large for the repository are downloaded once with
# curl (part of Windows 10/11, Linux and macOS).

include(../common.pri)

TEMPLATE = app
CONFIG  += console release warn_off
CONFIG  -= qt app_bundle warn_on debug
CONFIG  += object_parallel_to_source   # same file names in different folders

# Optimise for speed instead of Qt's default -O2.
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -O3
QMAKE_CFLAGS_RELEASE   -= -O2
QMAKE_CFLAGS_RELEASE   += -O3

# The engines do not use any mbChess code or settings.
DEFINES =
INCLUDEPATH =

# Windows: link the C++ runtime and winpthreads statically, so that the engine
# runs without MinGW DLLs next to it.
win32-g++: QMAKE_LFLAGS += -static -static-libgcc -static-libstdc++
unix:!macx: LIBS += -lpthread

# CPU features. Default: x86-64 with AVX2 (every Intel/AMD CPU since about 2015),
# Apple Silicon / ARM64 with NEON. Override for old CPUs with
#   qmake MBCHESS_ENGINE_CPU=sse41      (or: generic)
isEmpty(MBCHESS_ENGINE_CPU) {
    contains(QT_ARCH, x86_64): MBCHESS_ENGINE_CPU = avx2
    else: contains(QT_ARCH, arm64)|contains(QT_ARCH, aarch64): MBCHESS_ENGINE_CPU = neon
    else: MBCHESS_ENGINE_CPU = generic
}

# mbchessEngineDir(name): the engine goes to <program folder>/engines/<name>,
# its object files to obj/<name> in the build folder.
defineTest(mbchessEngineDir) {
    DESTDIR = $$MBCHESS_ENGINE_DIR/$$1
    OBJECTS_DIR = obj/$$1
    export(DESTDIR)
    export(OBJECTS_DIR)
    return(true)
}

# mbchessDownload(id, file, url1 [, url2 ...]): download a file once.
# It becomes a prerequisite of the program (app projects) or of the default
# target (aux projects). curl writes <file>.part, which is only renamed after a
# successful download, so an aborted download is simply retried by the next "make".
defineTest(mbchessDownload) {
    id   = $$1
    file = $$2
    urls = $$member(ARGS, 2, -1)
    part = $$shell_quote($$shell_path($${file}.part))
    cmds =
    for (url, urls) {
        !isEmpty(cmds): cmds += ||
        cmds += curl -L --fail --retry 2 -o $$part $$shell_quote($$url)
    }
    $${id}.target   = $$file
    # The brackets matter: cmd.exe reads "a || b && c" as "a || (b && c)".
    $${id}.commands = ( $$cmds ) && $(MOVE) $$part $$shell_quote($$shell_path($$file))
    QMAKE_EXTRA_TARGETS += $$id
    PRE_TARGETDEPS += $$file
    first.depends += $$file
    export($${id}.target)
    export($${id}.commands)
    export(QMAKE_EXTRA_TARGETS)
    export(PRE_TARGETDEPS)
    export(first.depends)
    return(true)
}
