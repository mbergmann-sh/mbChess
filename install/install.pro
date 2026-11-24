# mbChess - Windows installer as the last build step.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Only built with  qmake CONFIG+=installer  (see mbChess.pro). Runs
# make-installer.ps1, which deploys Qt with windeployqt and builds both
# installers with the Qt Installer Framework into install/output.

include(../common.pri)

TEMPLATE = aux
CONFIG  -= debug_and_release
QMAKE_EXTRA_TARGETS += first

win32 {
    script = $$shell_path($$PWD/make-installer.ps1)
    first.commands = powershell -NoProfile -ExecutionPolicy Bypass -File $$shell_quote($$script) \
        -BuildDir $$shell_quote($$shell_path($$MBCHESS_BUILD_ROOT)) \
        -QtBinDir $$shell_quote($$shell_path($$[QT_INSTALL_BINS]))
} else {
    first.commands = @echo "mbChess: the installer is only built on Windows"
}

OTHER_FILES += \
    make-installer.ps1 \
    make-installer.bat \
    README.md \
    config/controlscript.qs \
    packages/mbchess.app/meta/installscript.qs \
    packages/mbchess.manual/meta/installscript.qs \
    license/license_de.txt \
    license/license_en.txt
