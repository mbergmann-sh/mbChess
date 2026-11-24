#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Builds the spoken move announcements with the free text-to-speech system Piper.

German: voice "Thorsten" (CC0, Thorsten Müller); English: LibriTTS (CC BY 4.0), speaker 100.

    python3 -m venv venv && venv/bin/pip install piper-tts numpy
    curl -LO https://github.com/rhasspy/piper/releases/download/v0.0.2/voice-de-thorsten-low.tar.gz
    curl -LO https://github.com/rhasspy/piper/releases/download/v0.0.2/voice-en-us-libritts-high.tar.gz
    (unpack both)
    venv/bin/python tools/make-speech.py de de-thorsten-low.onnx src/app/resources/themes/sounds/thorsten-de
    venv/bin/python tools/make-speech.py en en-us-libritts-high.onnx src/app/resources/themes/sounds/libritts-en

The file names are the same as in the English set (sounds/amichess), so mbChess can use
either one. Afterwards run tools/clean-sounds.py on the folder (noise removal, 48 kHz;
for German with --brighten).
"""
import os
import subprocess
import sys

WORDS_EN = {
    'a': 'A.', 'b': 'B.', 'c': 'C.', 'd': 'D.', 'e': 'E.', 'f': 'F.', 'g': 'G.', 'h': 'H.',
    '1': 'one.', '2': 'two.', '3': 'three.', '4': 'four.', '5': 'five.', '6': 'six.', '7': 'seven.', '8': 'eight.',
    'pawn': 'Pawn.', 'knight': 'Knight.', 'bishop': 'Bishop.', 'rook': 'Rook.', 'queen': 'Queen.', 'king': 'King.',
    'takes': 'takes.', 'check': 'Check!', 'mate': 'Checkmate!', 'stalemate': 'Stalemate.', 'draw': 'Draw.',
    'o-o': 'Castles short.', 'o-o-o': 'Castles long.', 'castling': 'Castles.',
}
SPEAKER_EN = '100'

WORDS_DE = {
    'a': 'A.', 'b': 'Be.', 'c': 'Ze.', 'd': 'De.', 'e': 'E.', 'f': 'Eff.', 'g': 'Ge.', 'h': 'Ha.',
    '1': 'eins.', '2': 'zwei.', '3': 'drei.', '4': 'vier.', '5': 'fünf.', '6': 'sechs.', '7': 'sieben.', '8': 'acht.',
    'pawn': 'Bauer.', 'knight': 'Springer.', 'bishop': 'Läufer.', 'rook': 'Turm.', 'queen': 'Dame.', 'king': 'König.',
    'takes': 'schlägt.', 'check': 'Schach!', 'mate': 'Matt!', 'stalemate': 'Patt.', 'draw': 'Remis.',
    'o-o': 'Kurze Rochade.', 'o-o-o': 'Lange Rochade.', 'castling': 'Rochade.',
}

# Speaking speed (Piper's length scale, 1.0 = normal; larger = slower). The German
# letters and digits are short and easily confused (Be/De/Ge/E, fünf), so they are spoken a
# little slower;
# more than about 1.15 makes the voice sound dull and artificial. mbChess also leaves a
# short pause between the words.
LENGTH_DE = {w: 1.15 for w in 'abcdefgh12345678'}
LENGTH_DE_OTHER = 1.0


def main(language, model, target, speaker=None):
    words = WORDS_DE if language == 'de' else WORDS_EN
    if language == 'en' and speaker is None:
        speaker = SPEAKER_EN
    os.makedirs(target, exist_ok=True)
    piper = os.path.join(os.path.dirname(sys.executable), 'piper')
    for name, text in words.items():
        out = os.path.join(target, name + '.wav')
        # less generator noise than the defaults (0.667 / 0.8): a cleaner voice
        cmd = [piper, '-m', model, '--noise-scale', '0.333', '--noise-w', '0.333', '-f', out]
        if language == 'de':
            cmd += ['--length-scale', str(LENGTH_DE.get(name, LENGTH_DE_OTHER))]
        if speaker is not None:
            cmd += ['-s', str(speaker)]
        subprocess.run(cmd, input=text.encode('utf-8'), check=True,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    print('now run: python3 tools/clean-sounds.py', target, '--brighten' if language == 'de' else '')


if __name__ == '__main__':
    if len(sys.argv) < 4 or sys.argv[1] not in ('de', 'en'):
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else None)
