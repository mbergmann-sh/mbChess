#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Cleans up spoken words for mbChess: removes steady background noise (spectral gate),
cuts silence at both ends, resamples to 48 kHz and normalises the volume.

    python3 tools/clean-sounds.py <folder with .wav files> [--strength 1.5] [--brighten]

The files are changed in place (16 bit mono). Needs numpy.
Very short effects (under 0.15 s, e.g. tap.wav) are only resampled and normalised.
"""
import sys
import wave

import numpy as np

TARGET_RATE = 48000


def read(path):
    with wave.open(path) as w:
        rate, width, channels = w.getframerate(), w.getsampwidth(), w.getnchannels()
        raw = w.readframes(w.getnframes())
    if width == 1:
        x = (np.frombuffer(raw, np.uint8).astype(np.float64) - 128) / 128
    else:
        x = np.frombuffer(raw, np.int16).astype(np.float64) / 32768
    if channels > 1:
        x = x.reshape(-1, channels).mean(axis=1)
    return x, rate


def write(path, x, rate):
    peak = np.max(np.abs(x)) or 1.0
    y = np.clip(x / peak * 0.89, -1, 1)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes((y * 32767).astype('<i2').tobytes())


def stft(x, n, hop):
    win = np.hanning(n)
    pad = np.concatenate([np.zeros(n), x, np.zeros(n)])
    frames = [pad[i:i + n] * win for i in range(0, len(pad) - n, hop)]
    return np.fft.rfft(np.array(frames), axis=1), win


def istft(spec, n, hop, win, length):
    frames = np.fft.irfft(spec, n=n, axis=1)
    out = np.zeros(hop * len(frames) + n)
    norm = np.zeros_like(out)
    for i, f in enumerate(frames):
        out[i * hop:i * hop + n] += f * win
        norm[i * hop:i * hop + n] += win ** 2
    out /= np.maximum(norm, 1e-8)
    return out[n:n + length]


def denoise(x, rate, strength):
    n = 1024 if rate > 30000 else 512
    hop = n // 4
    spec, win = stft(x, n, hop)
    mag = np.abs(spec)
    energy = mag.sum(axis=1)
    quiet = mag[energy <= np.percentile(energy, 15)]
    noise = np.median(quiet, axis=0) if len(quiet) else np.zeros(mag.shape[1])
    gain = np.clip(1 - strength * noise / np.maximum(mag, 1e-9), 0.05, 1.0)
    # smooth the gain over time and frequency against "musical noise"
    k = np.ones((3, 3)) / 9
    padded = np.pad(gain, 1, mode='edge')
    gain = sum(padded[i:i + gain.shape[0], j:j + gain.shape[1]] * k[i, j] for i in range(3) for j in range(3))
    return istft(spec * gain, n, hop, win, len(x))


def brighten(x, rate):
    """Less dull: cuts rumble below 100 Hz and lifts the range from 2.5 kHz by about 7 dB (consonants)
    (the 16 kHz Piper voices have nothing above 8 kHz)."""
    spec = np.fft.rfft(x)
    f = np.fft.rfftfreq(len(x), 1 / rate)
    gain = np.where(f < 100, (f / 100) ** 2, 1.0)
    gain *= 1 + 1.2 / (1 + np.exp(-(f - 2500) / 400))
    return np.fft.irfft(spec * gain, len(x))


def trim(x, rate):
    env = np.abs(x)
    # low threshold and some margin: quiet consonants such as the f in "fünf" must stay
    threshold = 0.012 * env.max()
    idx = np.where(env > threshold)[0]
    if len(idx) == 0:
        return x
    pad = rate // 20
    return x[max(0, idx[0] - pad):min(len(x), idx[-1] + pad)]


def resample(x, rate, target):
    if rate == target:
        return x
    n = int(round(len(x) * target / rate))
    spec = np.fft.rfft(x)
    out = np.zeros(n // 2 + 1, dtype=complex)
    m = min(len(spec), len(out))
    out[:m] = spec[:m]
    return np.fft.irfft(out, n) * (n / len(x))


def fade(x, rate):
    f = min(len(x) // 4, rate // 200)  # 5 ms
    if f > 1:
        x[:f] *= np.linspace(0, 1, f)
        x[-f:] *= np.linspace(1, 0, f)
    return x


def main(folder, strength, bright=False):
    import glob
    import os
    for path in sorted(glob.glob(os.path.join(folder, '*.wav'))):
        x, rate = read(path)
        if len(x) / rate >= 0.15:
            x = denoise(x, rate, strength)
            x = trim(x, rate)
            if bright:
                x = brighten(x, rate)
        x = fade(resample(x, rate, TARGET_RATE), TARGET_RATE)
        write(path, x, TARGET_RATE)


if __name__ == '__main__':
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    s = float(sys.argv[sys.argv.index('--strength') + 1]) if '--strength' in sys.argv else 1.5
    main(sys.argv[1], s, '--brighten' in sys.argv)
