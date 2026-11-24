#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Imports piece sets and board themes from Lucas Chess (GPL) as mbChess themes.

    python3 tools/lucas2themes.py <source folder> <target folder>

Source: Pieces/<Name>/wk.svg ... bp.svg (+ author.txt, license.txt, LICENSE)
        Themes/*.lktheme3 (Lucas Chess board themes)
Target: pieces/lucas-<name>/wK.svg ... + theme.ini, and boards/<id>/theme.ini (+ light.png, dark.png)

The .lktheme3 files are Python pickles. They are read with an unpickler that refuses
every class and function, so a file can only yield plain data (dicts, lists, strings,
numbers), never run code.

Left out: Cburnett and CburnettSize (mbChess has cburnett already), board themes that
need Lucas Chess' full-board background picture, and themes named after football
clubs or chess websites. Themes with the same look are taken only once.
Needs Pillow (pip install pillow).
"""
import base64
import io
import os
import pickle
import re
import shutil
import sys

from PIL import Image

SKIP_PIECES = {'Cburnett', 'CburnettSize'}
SKIP_SECTIONS = {'Teams'}
SKIP_NAMES = {'chess.com', 'chesslecture.com', 'chessmaster', 'barcelona'}
MAX_TEXTURE = 300  # pixels; textures are stretched over one square


class SafeUnpickler(pickle.Unpickler):
    def find_class(self, module, name):
        raise pickle.UnpicklingError(f'refused: {module}.{name}')


def slug(text):
    return re.sub(r'[^a-z0-9]+', '-', text.lower()).strip('-')


def argb(value):
    """Qt ARGB integer -> #rrggbb"""
    return '#%06x' % (int(value) & 0xFFFFFF)


def ini_value(text):
    return text.replace('\\', '/').replace('\n', ' ')


def write_ini(path, values):
    with open(path, 'w', encoding='utf-8') as f:
        f.write('[theme]\n')
        for k, v in values.items():
            f.write(f'{k}={ini_value(str(v))}\n')


def import_pieces(src, dst):
    pieces = os.path.join(src, 'Pieces')
    count = 0
    for name in sorted(os.listdir(pieces)):
        folder = os.path.join(pieces, name)
        if name in SKIP_PIECES or not os.path.isdir(folder):
            continue
        files = {f.lower(): f for f in os.listdir(folder)}
        codes = [c + p for c in 'wb' for p in 'kqrbnp']
        if not all(c + '.svg' in files for c in codes):
            continue
        target = os.path.join(dst, 'pieces', 'lucas-' + slug(name))
        os.makedirs(target, exist_ok=True)
        for c in codes:
            shutil.copyfile(os.path.join(folder, files[c + '.svg']), os.path.join(target, c[0] + c[1].upper() + '.svg'))
        for extra in ('author.txt', 'license.txt', 'license', 'readme.md'):
            if extra in files:
                shutil.copyfile(os.path.join(folder, files[extra]), os.path.join(target, files[extra]))
        write_ini(os.path.join(target, 'theme.ini'), {'name': name, 'group': 'Lucas Chess'})
        count += 1
    return count


def texture(value):
    if not value:
        return None
    raw = value.encode('latin1') if isinstance(value, str) else bytes(value)
    if not raw.startswith(b'\x89PNG'):
        raw = base64.b64decode(raw)
    img = Image.open(io.BytesIO(raw))
    img.load()
    img = img.convert('RGBA')
    if max(img.size) > MAX_TEXTURE:
        img.thumbnail((MAX_TEXTURE, MAX_TEXTURE), Image.LANCZOS)
    return img


def import_boards(src, dst):
    themes_dir = os.path.join(src, 'Themes')
    seen = set()
    count = 0
    for file in sorted(os.listdir(themes_dir)):
        if not file.endswith('.lktheme3'):
            continue
        author = file[:-len('.lktheme3')]
        with open(os.path.join(themes_dir, file), 'rb') as f:
            themes = SafeUnpickler(f).load()
        for t in themes:
            o = t.get('o_tema', {})
            name = str(t.get('NOMBRE', '')).strip()
            if not name or t.get('SECCION') in SKIP_SECTIONS or name.lower() in SKIP_NAMES:
                continue
            light_tex = texture(o.get('x_png64Blancas'))
            dark_tex = texture(o.get('x_png64Negras'))
            if o.get('x_png64Fondo') and not (light_tex and dark_tex):
                continue  # depends on the full-board background
            key = (o.get('x_colorBlancas'), o.get('x_colorNegras'),
                   light_tex.tobytes() if light_tex else b'', dark_tex.tobytes() if dark_tex else b'')
            if key in seen:
                continue
            seen.add(key)
            ident = f'lucas-{slug(author)}-{slug(name)}'
            target = os.path.join(dst, 'boards', ident)
            os.makedirs(target, exist_ok=True)
            values = {
                'name': name,
                'group': f'Lucas Chess/{author}',
                'light': argb(o.get('x_colorBlancas', 0xFFF0D9B5)),
                'dark': argb(o.get('x_colorNegras', 0xFFB58863)),
                'frame': argb(o.get('x_colorExterior', 0xFF404040)),
                'coordinates': argb(o.get('x_colorTexto', 0xFF000000)),
                'texture': 'square',
            }
            if light_tex:
                light_tex.save(os.path.join(target, 'light.png'), optimize=True)
            if dark_tex:
                dark_tex.save(os.path.join(target, 'dark.png'), optimize=True)
            write_ini(os.path.join(target, 'theme.ini'), values)
            count += 1
    return count


def main(src, dst):
    print('piece sets:', import_pieces(src, dst))
    print('board themes:', import_boards(src, dst))


if __name__ == '__main__':
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    main(sys.argv[1], sys.argv[2])
