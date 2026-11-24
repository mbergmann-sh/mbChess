# Downloads Stockfish's neural network (about 100 MB) once into the build folder,
# where stockfish.pro embeds it into the program (incbin reads it relative to the
# build folder). The file name must match EvalFileDefaultName in
# stockfish/src/evaluate.h.
include(engine.pri)

TEMPLATE = aux
QMAKE_EXTRA_TARGETS += first

SF_NET = nn-1a298aa575a0.nnue
mbchessDownload(stockfish_net, $$OUT_PWD/$$SF_NET, \
    https://tests.stockfishchess.org/api/nn/$$SF_NET, \
    https://github.com/official-stockfish/networks/raw/master/$$SF_NET)
