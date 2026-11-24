# Shared settings for every mbChess sub-project.
# SPDX-License-Identifier: GPL-3.0-or-later

# --- Supported Qt versions: 5.15 (older macOS, e.g. Catalina) and 6.4+ ---
!versionAtLeast(QT_VERSION, 5.15.0) {
    error("mbChess benoetigt Qt 5.15 oder Qt 6.4+ / mbChess requires Qt 5.15 or Qt 6.4+ (found $$QT_VERSION)")
}
equals(QT_MAJOR_VERSION, 6):!versionAtLeast(QT_VERSION, 6.4.0) {
    error("mbChess benoetigt Qt 6.4 oder neuer / mbChess requires Qt 6.4 or newer (found $$QT_VERSION)")
}

CONFIG += c++17 warn_on
CONFIG -= debug_and_release debug_and_release_target

# Keep the code free of APIs that were already deprecated in Qt 5.15.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x050F00

MBCHESS_VERSION = 0.1.11
MBCHESS_CODENAME = Uppers and Downers
DEFINES += MBCHESS_VERSION=\\\"$$MBCHESS_VERSION\\\"

MBCHESS_ROOT       = $$PWD
# The engine wrappers (engines/wrapper.pri) pass the build root to the engines' own
# projects, because qmake cannot work it out for them.
isEmpty(MBCHESS_BUILD_ROOT): MBCHESS_BUILD_ROOT = $$shadowed($$PWD)
# $$shadowed() is empty when qmake runs on a subproject alone (Qt Creator: "Build" on
# a subproject such as tools/voicerecorder-build.pro or install/install.pro). Then the
# build folder of the subproject is <build root>/<path of the subproject>: go up.
isEmpty(MBCHESS_BUILD_ROOT) {
    mbc_rel = $$relative_path($$_PRO_FILE_PWD_, $$PWD)   # e.g. "tools" or "src/app"
    mbc_out = $$clean_path($$OUT_PWD)
    equals(mbc_rel, .) {
        MBCHESS_BUILD_ROOT = $$mbc_out
    } else:!contains(mbc_rel, ^\.\..*) {
        mbc_tail =
        for (part, $$list($$split(mbc_rel, /))) {
            mbc_tail = $$basename(mbc_out)/$$mbc_tail
            mbc_out  = $$dirname(mbc_out)
        }
        # only if the folders below match (not a separate build folder of the subproject)
        equals(mbc_tail, $$mbc_rel/): MBCHESS_BUILD_ROOT = $$mbc_out
    }
}
isEmpty(MBCHESS_BUILD_ROOT): error("mbChess: Build-Ordner unbekannt - bitte das ganze Projekt mbChess.pro bauen / unknown build folder - please build the whole project mbChess.pro")
MBCHESS_LIB_DIR    = $$MBCHESS_BUILD_ROOT/lib
MBCHESS_BIN_DIR    = $$MBCHESS_BUILD_ROOT/bin

# UCI engines live in the subfolder "engines" next to the program
# (inside the app bundle on macOS), one folder per engine.
macx: MBCHESS_ENGINE_DIR = $$MBCHESS_BIN_DIR/mbChess.app/Contents/MacOS/engines
else: MBCHESS_ENGINE_DIR = $$MBCHESS_BIN_DIR/engines

INCLUDEPATH += $$PWD/src/libs

macx {
    # Oldest supported macOS: 10.15 Catalina (requires the Qt 5.15 kit there)
    QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.15
}

# Link a static mbChess library: $$mbchessLinkLib(chesscore)
defineTest(mbchessLinkLib) {
    lib = $$1
    LIBS += -L$$MBCHESS_LIB_DIR -l$$lib
    msvc: PRE_TARGETDEPS += $$MBCHESS_LIB_DIR/$${lib}.lib
    else: PRE_TARGETDEPS += $$MBCHESS_LIB_DIR/lib$${lib}.a
    export(LIBS)
    export(PRE_TARGETDEPS)
    return(true)
}
