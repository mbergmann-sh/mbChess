# Wrapper project for one engine.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# The real project of each engine is engines/qmake/<name>.pro. It is not listed in
# SUBDIRS, because Qt Creator would then offer every engine as a program to run
# (and might start an engine instead of mbChess). Instead this "aux" project runs
# qmake and make for it as a build step. Qt Creator only sees the wrapper.
#
# Usage in engines/<name>.pro:   include(wrapper.pri)   and   mbchessEngineWrapper(<name>)

include(../common.pri)

TEMPLATE = aux
CONFIG  -= debug_and_release
QMAKE_EXTRA_TARGETS += first

defineTest(mbchessEngineWrapper) {
    name  = $$1
    inner = $$PWD/qmake/$${name}.pro
    mk    = Makefile.$${name}.inner

    args = -spec $$shell_quote($$QMAKESPEC) $$shell_quote(MBCHESS_BUILD_ROOT=$$MBCHESS_BUILD_ROOT)
    !isEmpty(MBCHESS_ENGINE_CPU): args += MBCHESS_ENGINE_CPU=$$MBCHESS_ENGINE_CPU

    # Re-run qmake for the engine only when its project files change
    $${name}_qmake.target   = $$mk
    $${name}_qmake.depends  = $$inner $$PWD/engine.pri $$MBCHESS_ROOT/common.pri
    $${name}_qmake.commands = $(QMAKE) -o $$mk $$shell_quote($$shell_path($$inner)) $$args

    # Always ask the engine's Makefile; it only compiles what is missing or changed
    $${name}_build.target   = $${name}_build
    $${name}_build.depends  = $$mk
    $${name}_build.commands = $(MAKE) -f $$mk

    first.depends += $${name}_build
    QMAKE_EXTRA_TARGETS += $${name}_qmake $${name}_build
    OTHER_FILES += $$inner

    export($${name}_qmake.target)
    export($${name}_qmake.depends)
    export($${name}_qmake.commands)
    export($${name}_build.target)
    export($${name}_build.depends)
    export($${name}_build.commands)
    export(first.depends)
    export(QMAKE_EXTRA_TARGETS)
    export(OTHER_FILES)
    return(true)
}
