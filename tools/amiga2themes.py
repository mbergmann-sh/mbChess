#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Converts Amiga chess graphics and sounds (IFF ILBM / 8SVX, e.g. from AmiChess) into
mbChess themes.

    python3 tools/amiga2themes.py <source folder> <target folder>

Source layout (as in AmiChess):
    Boards/<Name>/Light, Boards/<Name>/Dark   square textures (ILBM)
    Pieces/1024/<Name>                        strip of 12 pieces, black P R N B Q K then white (ILBM)
    Sounds/<word>                             spoken words and effects (8SVX)
Target layout (what mbChess reads, see src/app/themes.h):
    boards/<Name>/light.png, dark.png, theme.ini
    pieces/<Name>/wP.png ... bK.png, theme.ini
    sounds/<set>/<word>.wav                   lower-case names, 16-bit, normalised

Needs Pillow (pip install pillow).
"""
import os
import struct
import sys
import wave

from PIL import Image


def chunks(data):
    i = 12
    while i + 8 <= len(data):
        cid = data[i:i + 4].decode('latin1')
        n = struct.unpack('>I', data[i + 4:i + 8])[0]
        yield cid, data[i + 8:i + 8 + n]
        i += 8 + n + (n & 1)


def byterun1(body, size):
    out = bytearray()
    i = 0
    while len(out) < size and i < len(body):
        n = body[i]
        i += 1
        if n < 128:
            out += body[i:i + n + 1]
            i += n + 1
        elif n > 128:
            out += bytes([body[i]]) * (257 - n)
            i += 1
    return bytes(out)


def read_ilbm(data):
    c = dict(chunks(data))
    w, h, _x, _y, nplanes, mask, comp, _pad, transparent = struct.unpack('>HHhhBBBBH', c['BMHD'][:14])
    rowbytes = ((w + 15) // 16) * 2
    planes = nplanes + (1 if mask == 1 else 0)
    raw = c['BODY'] if comp == 0 else byterun1(c['BODY'], rowbytes * planes * h)
    cmap = c.get('CMAP', b'')
    palette = [tuple(cmap[i:i + 3]) for i in range(0, len(cmap) - 2, 3)]
    img = Image.new('RGBA', (w, h))
    px = img.load()
    for y in range(h):
        base = y * rowbytes * planes
        rows = [raw[base + p * rowbytes: base + (p + 1) * rowbytes] for p in range(planes)]
        for x in range(w):
            byte, bit = x >> 3, 7 - (x & 7)
            v = 0
            for p in range(nplanes):
                if rows[p][byte] >> bit & 1:
                    v |= 1 << p
            if nplanes == 24:
                r, g, b, a = v & 255, (v >> 8) & 255, (v >> 16) & 255, 255
            else:
                r, g, b = palette[v] if v < len(palette) else (0, 0, 0)
                a = 0 if (mask == 2 and v == transparent) else 255
            if mask == 1 and not (rows[nplanes][byte] >> bit & 1):
                a = 0
            px[x, y] = (r, g, b, a)
    return img


def write_8svx_as_wav(data, out):
    c = dict(chunks(data))
    oneshot, repeat, _spc, rate, _octaves, _compression = struct.unpack('>IIIHBB', c['VHDR'][:16])
    body = c['BODY'][:oneshot + repeat] if oneshot + repeat else c['BODY']
    samples = [b - 256 if b > 127 else b for b in body]
    peak = max(1, max(abs(v) for v in samples))
    gain = 0.9 * 32767 / peak  # the originals are quiet
    with wave.open(out, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b''.join(struct.pack('<h', int(v * gain)) for v in samples))


def write_ini(path, name):
    with open(path, 'w', encoding='utf-8') as f:
        f.write(f'[theme]\nname={name}\ngroup=AmiChess\n')


def is_iff(data, kind):
    return data[:4] == b'FORM' and data[8:12] == kind


def main(src, dst):
    boards = os.path.join(src, 'Boards')
    for name in sorted(os.listdir(boards)) if os.path.isdir(boards) else []:
        for part in ('Light', 'Dark'):
            data = open(os.path.join(boards, name, part), 'rb').read()
            if is_iff(data, b'ILBM'):
                os.makedirs(os.path.join(dst, 'boards', name), exist_ok=True)
                read_ilbm(data).convert('RGB').save(os.path.join(dst, 'boards', name, part.lower() + '.png'), optimize=True)
        write_ini(os.path.join(dst, 'boards', name, 'theme.ini'), name)

    pieces = os.path.join(src, 'Pieces', '1024')
    order = ['bP', 'bR', 'bN', 'bB', 'bQ', 'bK', 'wP', 'wR', 'wN', 'wB', 'wQ', 'wK']
    for name in sorted(os.listdir(pieces)) if os.path.isdir(pieces) else []:
        data = open(os.path.join(pieces, name), 'rb').read()
        if not is_iff(data, b'ILBM'):
            continue
        strip = read_ilbm(data)
        cell = strip.width // 12
        os.makedirs(os.path.join(dst, 'pieces', name), exist_ok=True)
        for i, code in enumerate(order):
            piece = strip.crop((i * cell, 0, (i + 1) * cell, strip.height))
            side = max(cell, strip.height)
            square = Image.new('RGBA', (side, side), (0, 0, 0, 0))
            square.paste(piece, ((side - cell) // 2, (side - strip.height) // 2))
            square.save(os.path.join(dst, 'pieces', name, code + '.png'), optimize=True)
        write_ini(os.path.join(dst, 'pieces', name, 'theme.ini'), 'AmiChess' if name == 'Default' else name)

    sounds = os.path.join(src, 'Sounds')
    for name in sorted(os.listdir(sounds)) if os.path.isdir(sounds) else []:
        data = open(os.path.join(sounds, name), 'rb').read()
        if is_iff(data, b'8SVX'):
            os.makedirs(os.path.join(dst, 'sounds', 'amichess'), exist_ok=True)
            write_8svx_as_wav(data, os.path.join(dst, 'sounds', 'amichess', name.lower() + '.wav'))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
