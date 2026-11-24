#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds src/app/resources/training/mate{1,2,3}.txt for Training > Find the mate.

Source: the Lichess puzzle database (CC0 1.0), https://database.lichess.org/#puzzles
  lichess_db_puzzle.csv.zst  (PuzzleId,FEN,Moves,Rating,RatingDeviation,Popularity,NbPlays,Themes,...)

Selection: puzzles with the theme mateIn<N>, popularity >= 85, played at least 300 times,
rating deviation <= 90; about 700 per N, spread evenly over the ratings (buckets of 100),
sorted by rating. The mbChess puzzles of tools/mate-puzzles-mbchess.txt come first.
Every puzzle is then checked with tools/puzzletool (the exact mate solver of chesscore):
the mate must be exactly N moves and the Lichess solution line must end in mate.

Usage:
  pip install zstandard
  python3 tools/make-mate-puzzles.py lichess_db_puzzle.csv.zst <path to mbchess-puzzletool>
"""
import collections
import csv
import io
import os
import random
import subprocess
import sys

import zstandard

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "src", "app", "resources", "training")
OWN = os.path.join(ROOT, "tools", "mate-puzzles-mbchess.txt")
PER_LEVEL = 700

HEADER = """# mbChess - Matt in {n} / mate in {n}
# Format: id;FEN;opponent move before the task (UCI, or -);solution (UCI);rating
# "mbchess:" puzzles: composed for mbChess (GPL-3.0-or-later, like the program).
# "lichess:" puzzles: Lichess puzzle database, https://database.lichess.org/ (CC0 1.0),
#   selected and checked by tools/make-mate-puzzles.py; https://lichess.org/training/<id>
"""


def read_lichess(path):
    found = {1: [], 2: [], 3: []}
    with open(path, "rb") as f:
        reader = csv.reader(io.TextIOWrapper(zstandard.ZstdDecompressor().stream_reader(f), encoding="utf-8"))
        next(reader)
        for row in reader:
            themes = row[7].split()
            for n in (1, 2, 3):
                if "mateIn%d" % n in themes and int(row[5]) >= 85 and int(row[6]) >= 300 and int(row[4]) <= 90:
                    found[n].append(row)
    return found


def select(rows):
    buckets = collections.defaultdict(list)
    for r in rows:
        buckets[min(int(r[3]) // 100, 25)].append(r)
    keys = sorted(k for k in buckets if len(buckets[k]) >= 60)
    per = PER_LEVEL // len(keys) + 1
    chosen = []
    for k in keys:
        chosen += random.sample(buckets[k], min(per, len(buckets[k])))
    chosen.sort(key=lambda r: int(r[3]))
    lines = []
    for r in chosen:
        moves = r[2].split()
        lines.append(";".join(["lichess:" + r[0], r[1], moves[0], " ".join(moves[1:]), r[3]]))
    return lines


def check(tool, n, lines):
    out = subprocess.run([tool, "4"], input="\n".join(lines) + "\n", capture_output=True, text=True,
                         check=True).stdout.splitlines()
    good = []
    for line, result in zip(lines, out):
        f = result.split()
        if f[1] == "OK" and int(f[2]) == n:
            good.append(line)
        else:
            print("dropped:", result, file=sys.stderr)
    return good


def main():
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    random.seed(2026)
    found = read_lichess(sys.argv[1])
    own = collections.defaultdict(list)
    with open(OWN, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith("#"):
                n, rest = line.split(";", 1)
                own[int(n)].append(rest)
    for n in (1, 2, 3):
        lines = check(sys.argv[2], n, own[n]) + check(sys.argv[2], n, select(found[n]))
        with open(os.path.join(OUT, "mate%d.txt" % n), "w", encoding="utf-8", newline="\n") as f:
            f.write(HEADER.format(n=n))
            f.write("\n".join(lines) + "\n")
        print("mate in %d: %d puzzles" % (n, len(lines)))


if __name__ == "__main__":
    main()
