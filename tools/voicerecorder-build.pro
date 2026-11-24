# Build step for the voice recorder (tools/voicerecorder), part of mbChess.pro.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The recorder is built into the program folder next to mbchess(.exe):
#   bin/mbchess-voicerecorder(.exe)     (macOS: bin/mbchess-voicerecorder.app)
# It is only built when that program does not exist yet. To build it again, delete it
# (or open tools/voicerecorder/voicerecorder.pro on its own in Qt Creator).
#
# Like the engines (see engines/wrapper.pri) this is an "aux" project that runs qmake
# and make for the real project, so that Qt Creator does not offer the recorder as a
# second program to run instead of mbChess.
# Needs Qt 6 with Qt Multimedia; otherwise it is skipped with a message.
# Leave it out with:  qmake CONFIG+=no_voicerecorder

include(../common.pri)

TEMPLATE = aux
CONFIG  -= debug_and_release
QMAKE_EXTRA_TARGETS += first

inner = $$PWD/voicerecorder/voicerecorder.pro
mk    = Makefile.voicerecorder.inner

lessThan(QT_MAJOR_VERSION, 6) {
    message("Sprachaufnahme wird uebersprungen: sie braucht Qt 6 / voice recorder skipped: needs Qt 6")
} else:!qtHaveModule(multimedia) {
    message("Sprachaufnahme wird uebersprungen: Qt Multimedia fehlt / voice recorder skipped: Qt Multimedia missing")
} else {
    win32:     exe = $$MBCHESS_BIN_DIR/mbchess-voicerecorder.exe
    else:macx: exe = $$MBCHESS_BIN_DIR/mbchess-voicerecorder.app/Contents/MacOS/mbchess-voicerecorder
    else:      exe = $$MBCHESS_BIN_DIR/mbchess-voicerecorder

    # No dependencies: make only runs this when the program is missing.
    recorder.target   = $$exe
    recorder.commands = $(QMAKE) -o $$mk $$shell_quote($$shell_path($$inner)) \
                            -spec $$shell_quote($$QMAKESPEC) \
                            $$shell_quote(DESTDIR=$$MBCHESS_BIN_DIR) \
                            $$shell_quote(OBJECTS_DIR=obj) $$shell_quote(MOC_DIR=moc) $$shell_quote(RCC_DIR=rcc) && \
                        $(MAKE) -f $$mk
    first.depends += $$exe
    QMAKE_EXTRA_TARGETS += recorder
}

OTHER_FILES += $$inner voicerecorder/README.md
