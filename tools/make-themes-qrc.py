#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Writes src/app/resources/themes.qrc: every theme file (png, svg, wav, theme.ini)
below src/app/resources/themes. Run it after adding or removing themes."""
import os

here = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'src', 'app', 'resources')
files = []
for root, _, names in os.walk(os.path.join(here, 'themes')):
    for n in names:
        if n.lower().endswith(('.png', '.svg', '.wav')) or n == 'theme.ini':
            files.append(os.path.relpath(os.path.join(root, n), here).replace(os.sep, '/'))
files.sort()
with open(os.path.join(here, 'themes.qrc'), 'w', encoding='utf-8') as f:
    f.write('<!DOCTYPE RCC>\n<RCC version="1.0">\n')
    f.write('    <!-- Built-in themes, written by tools/make-themes-qrc.py; see src/app/themes.h -->\n')
    f.write('    <qresource prefix="/">\n')
    for p in files:
        f.write(f'        <file>{p.replace("&", "&amp;")}</file>\n')
    f.write('    </qresource>\n</RCC>\n')
print(len(files), 'files')
