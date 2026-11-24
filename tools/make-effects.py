#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Synthesises mbChess' move sounds (a wooden piece set down, and a capture) into
src/app/resources/themes/sounds/effects. No recordings involved, so no third-party
rights. Needs numpy.

    python3 tools/make-effects.py [target folder]
"""
import os
import sys
import wave

import numpy as np

RATE = 48000


def knock(pitch=1.0, length=0.09, seed=1):
    """A wooden knock: a few damped resonances plus a tiny noise click."""
    rng = np.random.default_rng(seed)
    t = np.arange(int(RATE * length)) / RATE
    x = np.zeros_like(t)
    for freq, amp, decay in ((820, 1.0, 38), (1630, 0.55, 55), (2950, 0.30, 80), (4400, 0.15, 120)):
        f = freq * pitch * (1 + rng.uniform(-0.02, 0.02))
        x += amp * np.sin(2 * np.pi * f * t + rng.uniform(0, 6.28)) * np.exp(-decay * t)
    click = rng.normal(0, 1, len(t)) * np.exp(-900 * t)
    # soften the click with a short moving average (a simple low-pass)
    click = np.convolve(click, np.ones(6) / 6, mode='same')
    x += 0.6 * click
    attack = int(RATE * 0.0015)
    x[:attack] *= np.linspace(0, 1, attack)
    return x


def write(path, x):
    x = x / np.max(np.abs(x)) * 0.85
    pad = np.zeros(int(RATE * 0.01))
    x = np.concatenate([x, pad])
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(RATE)
        w.writeframes((x * 32767).astype('<i2').tobytes())


def main(target):
    os.makedirs(target, exist_ok=True)
    write(os.path.join(target, 'tap.wav'), knock(1.0, 0.09, seed=1))
    # capture: the taken piece knocked aside, then the own piece set down
    first = knock(1.25, 0.07, seed=2) * 0.7
    second = knock(0.92, 0.11, seed=3)
    gap = np.zeros(int(RATE * 0.055))
    write(os.path.join(target, 'hit.wav'), np.concatenate([first, gap, second]))


if __name__ == '__main__':
    here = os.path.dirname(os.path.abspath(__file__))
    main(sys.argv[1] if len(sys.argv) > 1 else
         os.path.join(here, '..', 'src', 'app', 'resources', 'themes', 'sounds', 'effects'))
