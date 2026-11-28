#!/usr/bin/env python3
# mbChess - packs a Linux build (bin/ of the build folder) into
# install/output/mbChess-<version>-linux-x86_64.tar.gz for the beta testers.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Usage:
#   python3 tools/make-linux-package.py --build-dir <build folder>
# or with qmake on Linux:  qmake CONFIG+=installer ...   (runs this script last)
#
# Upload the archive by hand into builds/linux/ of the beta download website
# (website/), the Windows installers from install/output into builds/windows/.

import argparse
import io
import re
import sys
import tarfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

LINUX_README = """mbChess {version} für Linux (Beta)
================================

Entpacken und starten:
    tar xzf mbChess-{version}-linux-x86_64.tar.gz
    cd mbChess-{version}
    ./mbchess

mbChess nutzt die Qt-Bibliotheken der Distribution (Qt 6.4 oder neuer).
Unter Debian 12 / Ubuntu 24.04 und neuer:
    sudo apt install libqt6widgets6 libqt6svg6 libqt6network6 \\
        libqt6serialport6 libqt6multimedia6 qt6-qpa-plugins

Die Schach-Engines liegen im Ordner engines/ und werden automatisch gefunden.
Diese Beta-Version läuft 6 Wochen nach dem ersten Start ab.

mbChess {version} for Linux (beta): unpack, install the Qt 6 packages above and
run ./mbchess. This beta expires 6 weeks after the first start.
"""

DATA_FOLDERS = {
    "Openings": "Eröffnungsbücher im PolyGlot-Format (*.bin) hierher kopieren.\n",
    "Chessbooks": "Partiesammlungen (*.pgn) hierher kopieren. mbChess baut daraus ein Buch, wie Crafty.\n",
    "Syzygy": "Endspiel-Datenbanken (Syzygy, *.rtbw/*.rtbz). mbChess lädt sie über Engines > Endspiel-Datenbanken herunter.\n",
    "Gaviota": "Endspiel-Datenbanken (Gaviota, *.gtb.cp4) hierher kopieren.\n",
}


def say(text):
    print(f"mbChess-Linux-Paket: {text}", flush=True)


def version():
    text = (ROOT / "common.pri").read_text(encoding="utf-8")
    m = re.search(r"(?m)^\s*MBCHESS_VERSION\s*=\s*(\S+)", text)
    if not m:
        sys.exit("mbChess-Linux-Paket: MBCHESS_VERSION not found in common.pri")
    return m.group(1)


def pack(ver, build_dir, out_dir):
    bin_dir = Path(build_dir) / "bin"
    if not (bin_dir / "mbchess").is_file():
        sys.exit(f"mbChess-Linux-Paket: {bin_dir / 'mbchess'} not found. Build mbChess first.")
    out_dir.mkdir(parents=True, exist_ok=True)
    name = f"mbChess-{ver}-linux-x86_64.tar.gz"
    top = f"mbChess-{ver}"
    tmp = out_dir / (name + ".part")

    def skip(info):
        parts = Path(info.name).parts
        # Crafty may not be redistributed; build leftovers stay out
        if len(parts) >= 3 and parts[1] == "engines" and parts[2].lower() == "crafty":
            return None
        if info.name.endswith((".o", ".a", "Makefile")):
            return None
        return info

    def add_text(tar, arcname, text):
        data = text.encode("utf-8")
        info = tarfile.TarInfo(f"{top}/{arcname}")
        info.size = len(data)
        info.mtime = int(time.time())
        info.mode = 0o644
        tar.addfile(info, io.BytesIO(data))

    with tarfile.open(tmp, "w:gz") as tar:
        for f in sorted(bin_dir.iterdir()):
            tar.add(f, arcname=f"{top}/{f.name}", filter=skip)
        for doc in ("LICENSE", "README.md", "CREDITS.md"):
            if (ROOT / doc).is_file():
                tar.add(ROOT / doc, arcname=f"{top}/{doc}")
        tar.add(ROOT / "dist" / "linux" / "mbchess.desktop", arcname=f"{top}/mbchess.desktop")
        tar.add(ROOT / "src" / "app" / "resources" / "icons" / "mbchess.svg", arcname=f"{top}/mbchess.svg")
        for folder, text in DATA_FOLDERS.items():
            if not (bin_dir / folder).exists():
                add_text(tar, f"{folder}/LIESMICH.txt", text)
        add_text(tar, "LIESMICH-LINUX.txt", LINUX_README.format(version=ver))
    tmp.replace(out_dir / name)
    say(f"{out_dir / name}")
    say("upload it into builds/linux/ of the beta download website")


def main():
    ap = argparse.ArgumentParser(description="Packs a Linux build of mbChess as .tar.gz.")
    ap.add_argument("--build-dir", required=True, help="mbChess build folder (contains bin/)")
    ap.add_argument("--out", default=str(ROOT / "install" / "output"), help="output folder")
    args = ap.parse_args()
    pack(version(), args.build_dir, Path(args.out))


if __name__ == "__main__":
    main()
