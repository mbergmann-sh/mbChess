# Unit tests. Run with "make check" from the build directory.
# SPDX-License-Identifier: GPL-3.0-or-later

TEMPLATE = subdirs
SUBDIRS += \
    tst_chesscore \
    tst_pgn \
    tst_book \
    tst_engine

qtHaveModule(serialport): SUBDIRS += tst_dgt
