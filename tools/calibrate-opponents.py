#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Measures the strength of the beginner opponents of the menu "Play" (src/app/opponents.cpp).

Texel's UCI_Elo (calibrated by its author, range -625..2900) is the yardstick: each
opponent plays a few matches against Texel at fixed Elo values, and its strength is the
Elo at which it would score 50 %. The built-in players (random mover, piece hunter) are
reimplemented here exactly as in opponents::builtinMove().

Usage (python-chess required):
  python3 tools/calibrate-opponents.py <folder with texel/ and patricia/> [games per match] [ms per move]
"""
import concurrent.futures
import math
import os
import random
import sys

import chess
import chess.engine

ENGINES = sys.argv[1] if len(sys.argv) > 1 else "bin/engines"
GAMES = int(sys.argv[2]) if len(sys.argv) > 2 else 20
MOVETIME = int(sys.argv[3]) if len(sys.argv) > 3 else 500
MAX_PLIES = 300

# Short openings (both colours each), so the games differ.
OPENINGS = ["e2e4 e7e5", "d2d4 d7d5", "e2e4 c7c5", "c2c4 e7e5", "g1f3 d7d5",
            "e2e4 e7e6", "d2d4 g8f6", "e2e4 c7c6", "b1c3 e7e5", "f2f4 d7d5"]
VALUE = {chess.PAWN: 1, chess.KNIGHT: 3, chess.BISHOP: 3, chess.ROOK: 5, chess.QUEEN: 9, chess.KING: 0}


def builtin_move(board, kind, rng):
    moves = list(board.legal_moves)
    if kind == "capturer":
        best, cands = 0, []
        for m in moves:
            gain = 0
            if board.is_en_passant(m):
                gain = 1
            elif board.piece_at(m.to_square):
                gain = VALUE[board.piece_at(m.to_square).piece_type]
            if m.promotion:
                gain += VALUE[m.promotion] - 1
            if gain > best:
                best, cands = gain, []
            if gain == best and gain > 0:
                cands.append(m)
        if cands:
            return rng.choice(cands)
    return rng.choice(moves)


class Player:
    def __init__(self, spec):
        self.spec = spec
        self.engine = None
        kind, _, value = spec.partition("@")
        self.kind, self.value = kind, value
        if kind == "texel":
            self.engine = chess.engine.SimpleEngine.popen_uci(os.path.join(ENGINES, "texel", "texel"))
            self.engine.configure({"UCI_LimitStrength": True, "UCI_Elo": int(value), "Threads": 1})
        elif kind == "patricia":
            self.engine = chess.engine.SimpleEngine.popen_uci(os.path.join(ENGINES, "patricia", "patricia"))
            self.engine.configure({"Skill_Level": int(value), "Threads": 1})

    def move(self, board, rng):
        if self.engine is None:
            return builtin_move(board, self.kind, rng)
        return self.engine.play(board, chess.engine.Limit(time=MOVETIME / 1000)).move

    def close(self):
        if self.engine:
            self.engine.quit()


def play(white, black, opening, seed):
    rng = random.Random(seed)
    board = chess.Board()
    for u in opening.split():
        board.push_uci(u)
    players = {chess.WHITE: Player(white), chess.BLACK: Player(black)}
    try:
        while not board.is_game_over(claim_draw=True) and board.ply() < MAX_PLIES:
            board.push(players[board.turn].move(board, rng))
    finally:
        for p in players.values():
            p.close()
    res = board.result(claim_draw=True)
    return {"1-0": 1.0, "0-1": 0.0}.get(res, 0.5)


def match(a, b, games):
    """Score of a against b."""
    jobs = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=os.cpu_count()) as ex:
        for i in range(games):
            op = OPENINGS[(i // 2) % len(OPENINGS)]
            if i % 2 == 0:
                jobs.append((ex.submit(play, a, b, op, i), True))
            else:
                jobs.append((ex.submit(play, b, a, op, i), False))
        score = sum(f.result() if a_white else 1 - f.result() for f, a_white in jobs)
    return score


def elo_diff(score, games):
    s = min(max(score / games, 0.5 / games), 1 - 0.5 / games)
    return -400 * math.log10(1 / s - 1)


def main():
    plan = [l.split() for l in os.environ.get("PLAN", "").split(";") if l.strip()] or [
        ["random", "texel@-400", "texel@-200", "texel@0"],
        ["capturer", "texel@0", "texel@200", "texel@400"],
        ["patricia@2", "texel@600", "texel@800", "texel@1000"],
        ["patricia@3", "texel@800", "texel@1000", "texel@1200"],
    ]
    for row in plan:
        who, refs = row[0], row[1:]
        estimates = []
        for ref in refs:
            score = match(who, ref, GAMES)
            elo = int(ref.split("@")[1]) + elo_diff(score, GAMES)
            estimates.append(elo)
            print(f"{who:12} vs {ref:12}: {score:4.1f}/{GAMES} -> about {elo:5.0f} Elo", flush=True)
        print(f"{who:12} = about {sum(estimates) / len(estimates):.0f} Elo (mean)\n", flush=True)


if __name__ == "__main__":
    main()
