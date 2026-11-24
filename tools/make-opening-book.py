#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds mbChess' own PolyGlot opening book from the named opening lines of lichess
(chess-openings, CC0, https://github.com/lichess-org/chess-openings).

    pip install chess
    python3 tools/make-opening-book.py src/app/resources/openings src/app/resources/books/mbchess.bin

Every move of every line counts once. A move that leads into many named lines is a main
line, so its weight is the square of that count; moves that only one line follows are left
out where the position has clearly better known moves.
"""
import collections
import glob
import os
import struct
import sys

import chess
import chess.polyglot


def encode(board, move):
    to = move.to_square
    if board.is_castling(move):
        to = chess.square(7 if chess.square_file(move.to_square) > chess.square_file(move.from_square) else 0,
                          chess.square_rank(move.from_square))
    promo = move.promotion - 1 if move.promotion else 0  # knight=1 ... queen=4
    return (chess.square_file(to) | chess.square_rank(to) << 3 | chess.square_file(move.from_square) << 6
            | chess.square_rank(move.from_square) << 9 | promo << 12)


def main(folder, target):
    counts = collections.defaultdict(collections.Counter)
    for path in sorted(glob.glob(os.path.join(folder, '*.tsv'))):
        with open(path, encoding='utf-8') as f:
            for line in f:
                cols = line.rstrip('\n').split('\t')
                if len(cols) < 3 or cols[0] == 'eco':
                    continue
                board = chess.Board()
                for token in cols[2].split():
                    if token.endswith('.'):
                        continue
                    move = board.parse_san(token)
                    counts[chess.polyglot.zobrist_hash(board)][encode(board, move)] += 1
                    board.push(move)
    entries = []
    for key, moves in counts.items():
        best = max(moves.values())
        for code, n in moves.items():
            if n == 1 and best >= 4:
                continue
            entries.append((key, code, min(65535, n * n)))
    entries.sort()
    with open(target, 'wb') as f:
        for key, code, weight in entries:
            f.write(struct.pack('>QHHI', key, code, weight, 0))
    print(len(counts), 'positions,', len(entries), 'entries')


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
