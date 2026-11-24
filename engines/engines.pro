# Free UCI engines bundled with mbChess, built from source.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Every engine ends up in <program folder>/engines/<name>/.
# Choose engines with   qmake "MBCHESS_ENGINES=stockfish fruit"
# Crafty (XBoard) is downloaded and compiled at build time, because its licence
# allows personal use only: its source is not part of mbChess, and the Windows
# installer does not ship it (see crafty.pro).

TEMPLATE = subdirs

isEmpty(MBCHESS_ENGINES): MBCHESS_ENGINES = stockfish arasan fruit toga glaurung gnuchess viridithas crafty

for (e, MBCHESS_ENGINES) {
    equals(e, viridithas) {
        # Rust engine: only when cargo is available
        win32: CARGO = $$system(where cargo 2>nul)
        else:  CARGO = $$system(command -v cargo 2>/dev/null)
        isEmpty(CARGO) {
            message("Viridithas wird uebersprungen: Rust (cargo) nicht gefunden, siehe https://rustup.rs")
            next()
        }
    }
    equals(e, gnuchess):win32 {
        message("GNU Chess wird unter Windows nicht gebaut (benoetigt POSIX), siehe engines/README.md")
        next()
    }
    SUBDIRS += $$e
    $${e}.file = $${e}.pro
    equals(e, stockfish) {
        # the network has to be there before Stockfish is compiled
        SUBDIRS += stockfish_net
        stockfish_net.file = stockfish_net.pro
        stockfish.depends = stockfish_net
    }
}

OTHER_FILES += README.md engine.pri crafty.pro
