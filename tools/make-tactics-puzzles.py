#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds src/app/resources/training/tactics.txt for Training > Tactics.

Source: the Lichess puzzle database (CC0 1.0), https://database.lichess.org/#puzzles
Selection: no mates (those are in mate<N>.txt), popularity >= 88, played at least 500
times, rating deviation <= 85. For each theme of THEMES about 150 puzzles, spread evenly
over the ratings (buckets of 200 from 400 to 2600). Every line is checked with
python-chess: the moves must be legal. Each puzzle keeps all of its themes that are in
THEMES, so it shows up under each of them.

Usage:
  pip install zstandard chess
  python3 tools/make-tactics-puzzles.py lichess_db_puzzle.csv.zst
"""
import collections
import csv
import io
import os
import random
import sys

import chess
import zstandard

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "app", "resources", "training", "tactics.txt")
PER_THEME = 150
# The same list (and order) as tacticThemes() in src/app/trainer.cpp
THEMES = ["hangingPiece", "fork", "pin", "skewer", "discoveredAttack", "doubleCheck", "trappedPiece",
          "deflection", "attraction", "sacrifice", "capturingDefender", "clearance", "interference",
          "intermezzo", "xRayAttack", "quietMove", "defensiveMove", "zugzwang", "promotion",
          "kingsideAttack", "pawnEndgame", "rookEndgame"]

HEADER = """# mbChess - Taktik / tactics
# Format: id;FEN;opponent move before the task (UCI);solution (UCI);rating;themes
# Lichess puzzle database, https://database.lichess.org/ (CC0 1.0),
#   selected by tools/make-tactics-puzzles.py; https://lichess.org/training/<id>
"""


def legal(row):
    board = chess.Board(row[1])
    try:
        for u in row[2].split():
            board.push(board.parse_uci(u))
    except ValueError:
        return False
    return True


def main():
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    random.seed(2026)
    by_theme = collections.defaultdict(list)
    with open(sys.argv[1], "rb") as f:
        reader = csv.reader(io.TextIOWrapper(zstandard.ZstdDecompressor().stream_reader(f), encoding="utf-8"))
        next(reader)
        for row in reader:
            themes = row[7].split()
            if "mate" in themes or int(row[5]) < 88 or int(row[6]) < 500 or int(row[4]) > 85:
                continue
            for t in themes:
                if t in THEMES:
                    by_theme[t].append(row)
    chosen = {}
    for t in THEMES:
        buckets = collections.defaultdict(list)
        for r in by_theme[t]:
            rating = int(r[3])
            if 400 <= rating < 2600:
                buckets[rating // 200].append(r)
        have = sum(1 for r in chosen.values() if t in r[7].split())
        need = max(0, PER_THEME - have)
        keys = sorted(buckets)
        per = need // max(1, len(keys)) + 1
        for k in keys:
            pool = [r for r in buckets[k] if r[0] not in chosen]
            for r in random.sample(pool, min(per, len(pool))):
                if legal(r):
                    chosen[r[0]] = r
    rows = sorted(chosen.values(), key=lambda r: int(r[3]))
    with open(OUT, "w", encoding="utf-8", newline="\n") as f:
        f.write(HEADER)
        for r in rows:
            moves = r[2].split()
            themes = " ".join(t for t in r[7].split() if t in THEMES)
            f.write(";".join(["lichess:" + r[0], r[1], moves[0], " ".join(moves[1:]), r[3], themes]) + "\n")
    count = collections.Counter(t for r in rows for t in r[7].split() if t in THEMES)
    print(len(rows), "puzzles;", ", ".join("%s %d" % (t, count[t]) for t in THEMES))


if __name__ == "__main__":
    main()
