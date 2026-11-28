#!/usr/bin/env python3
# mbChess - builds a Debian package from a Linux build (bin/ of the build folder):
#   install/output/mbchess_<version>_<arch>.deb
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Usage:
#   python3 tools/make-deb.py --build-dir <build folder>
# or with qmake on Linux:  qmake6 CONFIG+=installer ...   (runs this script last,
# together with tools/make-linux-package.py)
#
# Install the package with
#   sudo apt install ./install/output/mbchess_<version>_amd64.deb
# (apt also installs the Qt 6 libraries it needs). Remove it with  sudo apt remove mbchess
#
# Layout of the package:
#   /opt/mbChess/                    the program folder, as bin/ of the build:
#     mbchess, mbchess-voicerecorder, engines/ (without Crafty), translations/,
#     Handbuch/, Manual/, themes/, Openings/, Chessbooks/, Syzygy/, Gaviota/, Training/
#   /usr/bin/mbchess                 -> /opt/mbChess/mbchess
#   /usr/bin/mbchess-voicerecorder   -> /opt/mbChess/mbchess-voicerecorder
#   /usr/share/applications/mbchess.desktop, icons in /usr/share/icons/hicolor
#   /usr/share/doc/mbchess/          copyright, changelog, manuals (links)
#   /usr/share/man/man6/             mbchess.6, mbchess-voicerecorder.6
# The data folders in /opt/mbChess (Openings, Chessbooks, ...) are writable for all
# users (like /tmp, mode 1777), so that books can be copied there and tablebases can be
# downloaded without root.
#
# Missing build packages:  sh tools/install-build-deps.sh   (or make-deb.py --install-deps)
#
# Needs dpkg-deb (package dpkg, always there on Debian/Ubuntu). With dpkg-shlibdeps
# (package dpkg-dev) the dependencies are worked out from the programs, otherwise a
# fixed list for Qt 6 (Debian 12/13, Ubuntu 24.04) is used. strip (binutils) is used
# when present.

import argparse
import gzip
import os
import re
import shutil
import stat
import subprocess
import sys
import tempfile
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent

PACKAGE = "mbchess"
PREFIX = "opt/mbChess"
# GitHub no-reply address; put your own address here if you like
MAINTAINER = "Michael Bergmann <23148422+mbergmann-sh@users.noreply.github.com>"
HOMEPAGE = "https://github.com/mbergmann-sh/mbChess"

# Folders the user fills; writable for everyone (sticky, as /tmp).
DATA_FOLDERS = ["Openings", "Chessbooks", "Syzygy", "Gaviota", "Training"]

# Used when dpkg-shlibdeps is not installed. Qt 6.4 is the oldest Qt 6 mbChess builds with.
FALLBACK_DEPENDS = [
    "libc6", "libstdc++6", "libgcc-s1",
    "libqt6core6 (>= 6.4)", "libqt6gui6 (>= 6.4)", "libqt6widgets6 (>= 6.4)",
    "libqt6network6 (>= 6.4)", "libqt6svg6 (>= 6.4)",
    "libqt6serialport6 (>= 6.4)", "libqt6multimedia6 (>= 6.4)",
]
# Loaded at run time (plugins), never seen by dpkg-shlibdeps.
# qt6-qpa-plugins: the X11/Wayland platform plugins; libqt6svg6: SVG icons and images
# (plugins); libssl3: TLS for downloads (tablebases, Lichess), loaded by Qt at run time.
EXTRA_DEPENDS = ["qt6-qpa-plugins", "libqt6svg6 (>= 6.4)", "libssl3t64 | libssl3"]
# Installed by apt by default as well: sound output through GStreamer (Qt 6.4 uses it
# on Linux), more image formats, xdg-open for the PDF manual and web links.
RECOMMENDS = ["gstreamer1.0-plugins-base", "gstreamer1.0-plugins-good",
              "qt6-image-formats-plugins", "xdg-utils"]

# lintian: mbChess is a third-party package and lives in /opt on purpose; the Gaviota
# tablebase code brings its own copy of zlib; the program is in /opt, not /usr/games.
LINTIAN_OVERRIDES = """\
mbchess: dir-or-file-in-opt [opt/*]
mbchess: embedded-library zlib [opt/mbChess/mbchess]
mbchess: package-section-games-but-contains-no-game
"""

MANPAGE = """.TH {name} 6 "" "mbChess {version}" "Games"
.SH NAME
{name} \\- {what}
.SH SYNOPSIS
.B {name}
.SH DESCRIPTION
{text}
The manual (PDF) is in /usr/share/doc/mbchess and in the Help menu of mbChess.
.SH FILES
/opt/mbChess (program, engines, data folders Openings, Chessbooks, Syzygy, Gaviota, Training)
.br
~/.config/mbChess/mbChess.conf (settings)
"""
MANPAGES = {
    "mbchess": ("chess program with free UCI engines",
                "mbChess plays and analyses chess with free UCI engines, reads and writes PGN "
                "game databases and offers training and DGT e-board support."),
    "mbchess-voicerecorder": ("records the spoken move announcements for mbChess",
                              "The voice recorder of mbChess records your own voice for the "
                              "spoken moves and messages of mbChess."),
}

DESCRIPTION = """\
chess program with free UCI engines (Schachprogramm)
 mbChess plays and analyses chess with free UCI engines (Stockfish, Fruit,
 Toga, GNU Chess, Arasan, Viridithas and more are included; other UCI
 engines can be added). It reads and writes PGN game databases, uses
 PolyGlot opening books and endgame tablebases (Syzygy, Gaviota), offers
 opponents for every level, rated games, training (mates, tactics,
 endgames, openings, master games), a tutor and DGT e-board support.
 The program speaks German and English."""

POSTINST = """#!/bin/sh
set -e
if [ "$1" = "configure" ]; then
    for d in {folders}; do
        mkdir -p "/{prefix}/$d"
        chmod 1777 "/{prefix}/$d"
    done
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
    if command -v gtk-update-icon-cache >/dev/null 2>&1; then
        gtk-update-icon-cache -q -t /usr/share/icons/hicolor || true
    fi
fi
exit 0
"""

POSTRM = """#!/bin/sh
set -e
if [ "$1" = "remove" ] || [ "$1" = "purge" ]; then
    if command -v update-desktop-database >/dev/null 2>&1; then
        update-desktop-database -q /usr/share/applications || true
    fi
fi
if [ "$1" = "purge" ]; then
    # books, tablebases and puzzles the users copied into the program folder
    rm -rf "/{prefix}"
fi
exit 0
"""

COPYRIGHT = """Format: https://www.debian.org/doc/packaging-manuals/copyright-format/1.0/
Upstream-Name: mbChess
Source: {homepage}

Files: *
Copyright: 2026 Michael Bergmann
License: GPL-3+

Comment: mbChess ships free chess engines (Stockfish, Fruit, Toga, GNU Chess,
 Arasan, Viridithas, Texel, Patricia, Tarrasch, Glaurung) in /opt/mbChess/engines,
 each under its own free licence (mostly GPL-3+), and themes from AmiChess and
 Lucas Chess (GPL). See CREDITS.md in this folder. Crafty is not included.

License: GPL-3+
 This program is free software: you can redistribute it and/or modify it
 under the terms of the GNU General Public License as published by the Free
 Software Foundation, either version 3 of the License, or (at your option)
 any later version.
 .
 On Debian systems, the complete text of the GNU General Public License
 version 3 can be found in "/usr/share/common-licenses/GPL-3".
"""


def say(text):
    print(f"mbChess-Debian-Paket: {text}", flush=True)


def version():
    text = (ROOT / "common.pri").read_text(encoding="utf-8")
    m = re.search(r"(?m)^\s*MBCHESS_VERSION\s*=\s*(\S+)", text)
    if not m:
        sys.exit("mbChess-Debian-Paket: MBCHESS_VERSION not found in common.pri")
    return m.group(1)


def architecture():
    try:
        return subprocess.run(["dpkg", "--print-architecture"], capture_output=True,
                              text=True, check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return "amd64"


def is_elf(path):
    try:
        with open(path, "rb") as f:
            return f.read(4) == b"\x7fELF"
    except OSError:
        return False


def skip(rel):
    """Files of bin/ that stay out of the package."""
    parts = rel.parts
    # Crafty may not be redistributed
    if len(parts) >= 2 and parts[0] == "engines" and parts[1].lower().startswith("crafty"):
        return True
    if rel.name.endswith((".o", ".a", ".part")) or rel.name.startswith("Makefile"):
        return True
    return False


def copy_program(bin_dir, dest, strip_tool):
    """Copies bin/ to dest, strips ELF files, returns the list of ELF executables."""
    elves = []
    for src in sorted(bin_dir.rglob("*")):
        rel = src.relative_to(bin_dir)
        if skip(rel) or any(skip(Path(*rel.parts[:i])) for i in range(1, len(rel.parts))):
            continue
        target = dest / rel
        if src.is_symlink():
            target.parent.mkdir(parents=True, exist_ok=True)
            os.symlink(os.readlink(src), target)
        elif src.is_dir():
            target.mkdir(parents=True, exist_ok=True)
            target.chmod(0o755)
        elif src.is_file():
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, target)
            if is_elf(target):
                if strip_tool:
                    subprocess.run([strip_tool, "--strip-unneeded", "--remove-section=.comment",
                                    "--remove-section=.note", str(target)], check=False)
                exe = os.access(src, os.X_OK)
                target.chmod(0o755 if exe else 0o644)
                if exe:
                    elves.append(target)
            else:
                target.chmod(0o755 if os.access(src, os.X_OK) else 0o644)
    return elves


def shlib_depends(elves, work):
    """Dependencies from dpkg-shlibdeps, or None when it is missing or fails."""
    if not shutil.which("dpkg-shlibdeps") or not elves:
        return None
    deb = work / "debian"
    deb.mkdir()
    (deb / "control").write_text(f"Source: {PACKAGE}\n\nPackage: {PACKAGE}\nArchitecture: any\n",
                                 encoding="utf-8")
    res = subprocess.run(["dpkg-shlibdeps", "-O", "--ignore-missing-info",
                          "-x" + PACKAGE] + [str(e) for e in elves],
                         cwd=work, capture_output=True, text=True)
    if res.returncode != 0:
        say("dpkg-shlibdeps failed, using the fixed list of dependencies:")
        print(res.stderr.strip(), file=sys.stderr)
        return None
    m = re.search(r"shlibs:Depends=(.*)", res.stdout)
    if not m:
        return None
    return [d.strip() for d in m.group(1).split(",") if d.strip()]


def merge_depends(found):
    names = {re.split(r"[ (|]", d)[0] for d in found}
    out = list(found)
    for d in EXTRA_DEPENDS:
        first = re.split(r"[ (|]", d)[0]
        # libqt6svg6 may already be there from dpkg-shlibdeps
        if first not in names and first.replace("t64", "") not in names:
            out.append(d)
    return out


def install_deps(check_only=False):
    """Runs tools/install-build-deps.sh (lists and installs missing build packages)."""
    script = ROOT / "tools" / "install-build-deps.sh"
    args = ["sh", str(script)] + (["--check"] if check_only else [])
    return subprocess.run(args).returncode == 0


def offer_deps():
    """Missing packaging tools: offer to install them when somebody is at the terminal."""
    missing = [t for t in ("dpkg-shlibdeps", "strip") if not shutil.which(t)]
    if not missing:
        return
    say(f"not found: {', '.join(missing)} (packages dpkg-dev, binutils)")
    if sys.stdin.isatty():
        answer = input("mbChess-Debian-Paket: run tools/install-build-deps.sh now? [J/n] ")
        if answer.strip().lower() not in ("n", "nein", "no"):
            install_deps()
    else:
        say("install them with:  sh tools/install-build-deps.sh")


def write_gz(path, text):
    # -n style: no file name, fixed mtime -> same bytes for the same text
    with open(path, "wb") as raw:
        with gzip.GzipFile(filename="", mode="wb", fileobj=raw, compresslevel=9, mtime=0) as gz:
            gz.write(text.encode("utf-8"))
    path.chmod(0o644)


def desktop_file():
    text = (ROOT / "dist" / "linux" / "mbchess.desktop").read_text(encoding="utf-8")
    return text


def build(ver, build_dir, out_dir, keep):
    bin_dir = Path(build_dir) / "bin"
    if not (bin_dir / "mbchess").is_file():
        sys.exit(f"mbChess-Debian-Paket: {bin_dir / 'mbchess'} not found. Build mbChess first.")
    if not shutil.which("dpkg-deb"):
        sys.exit("mbChess-Debian-Paket: dpkg-deb not found (Debian/Ubuntu: package dpkg).")
    offer_deps()
    arch = architecture()
    out_dir.mkdir(parents=True, exist_ok=True)
    name = f"{PACKAGE}_{ver}_{arch}.deb"

    work = Path(tempfile.mkdtemp(prefix="mbchess-deb-"))
    try:
        os.umask(0o022)
        pkg = work / "pkg"
        prog = pkg / PREFIX
        prog.mkdir(parents=True)
        strip_tool = shutil.which("strip")
        elves = copy_program(bin_dir, prog, strip_tool)
        for folder in DATA_FOLDERS:
            (prog / folder).mkdir(exist_ok=True)
        if not (prog / "mbchess-voicerecorder").exists():
            say("note: no mbchess-voicerecorder in bin/ (needs Qt Multimedia), package without it")

        # launchers
        usr_bin = pkg / "usr" / "bin"
        usr_bin.mkdir(parents=True)
        for exe in ("mbchess", "mbchess-voicerecorder"):
            if (prog / exe).is_file():
                os.symlink(f"/{PREFIX}/{exe}", usr_bin / exe)

        # menu entry and icons
        apps = pkg / "usr" / "share" / "applications"
        apps.mkdir(parents=True)
        (apps / "mbchess.desktop").write_text(desktop_file(), encoding="utf-8")
        icons = ROOT / "src" / "app" / "resources" / "icons"
        svg_dir = pkg / "usr" / "share" / "icons" / "hicolor" / "scalable" / "apps"
        svg_dir.mkdir(parents=True)
        shutil.copyfile(icons / "mbchess.svg", svg_dir / "mbchess.svg")
        if (icons / "mbchess.png").is_file():
            size = png_size(icons / "mbchess.png")
            if size:
                png_dir = pkg / "usr" / "share" / "icons" / "hicolor" / f"{size}x{size}" / "apps"
                png_dir.mkdir(parents=True)
                shutil.copyfile(icons / "mbchess.png", png_dir / "mbchess.png")

        # manual pages and lintian overrides
        man = pkg / "usr" / "share" / "man" / "man6"
        man.mkdir(parents=True)
        for exe, (what, text) in MANPAGES.items():
            if (prog / exe).is_file():
                write_gz(man / f"{exe}.6.gz",
                         MANPAGE.format(name=exe, version=ver, what=what, text=text))
        ovr = pkg / "usr" / "share" / "lintian" / "overrides"
        ovr.mkdir(parents=True)
        (ovr / PACKAGE).write_text(LINTIAN_OVERRIDES, encoding="utf-8")

        # documentation
        doc = pkg / "usr" / "share" / "doc" / PACKAGE
        doc.mkdir(parents=True)
        (doc / "copyright").write_text(COPYRIGHT.format(homepage=HOMEPAGE), encoding="utf-8")
        stamp = time.strftime("%a, %d %b %Y %H:%M:%S +0000", time.gmtime())
        write_gz(doc / "changelog.gz",
                 f"{PACKAGE} ({ver}) unstable; urgency=medium\n\n"
                 f"  * mbChess {ver}, see README.md.\n\n -- {MAINTAINER}  {stamp}\n")
        for f in ("README.md", "CREDITS.md"):
            if (ROOT / f).is_file():
                write_gz(doc / (f + ".gz"), (ROOT / f).read_text(encoding="utf-8"))
        for manual in ("Handbuch/mbChess-Handbuch.pdf", "Manual/mbChess-Manual.pdf"):
            if (prog / manual).is_file():
                os.symlink(f"/{PREFIX}/{manual}", doc / Path(manual).name)

        for p in pkg.rglob("*"):
            if p.is_file() and not p.is_symlink() and not os.access(p, os.X_OK):
                p.chmod(0o644)
            elif p.is_dir():
                p.chmod(0o755)

        # control files
        depends = shlib_depends(elves, work)
        if depends is None:
            depends = list(FALLBACK_DEPENDS)
        depends = merge_depends(depends)
        size_kb = sum(p.stat().st_size for p in pkg.rglob("*")
                      if p.is_file() and not p.is_symlink()) // 1024
        control_dir = pkg / "DEBIAN"
        control_dir.mkdir()
        control = (f"Package: {PACKAGE}\n"
                   f"Version: {ver}\n"
                   f"Architecture: {arch}\n"
                   f"Maintainer: {MAINTAINER}\n"
                   f"Installed-Size: {size_kb}\n"
                   f"Depends: {', '.join(depends)}\n"
                   f"Recommends: {', '.join(RECOMMENDS)}\n"
                   f"Section: games\n"
                   f"Priority: optional\n"
                   f"Homepage: {HOMEPAGE}\n"
                   f"Description: {DESCRIPTION}\n")
        (control_dir / "control").write_text(control, encoding="utf-8")
        folders = " ".join(DATA_FOLDERS)
        for script, text in (("postinst", POSTINST), ("postrm", POSTRM)):
            path = control_dir / script
            path.write_text(text.format(folders=folders, prefix=PREFIX), encoding="utf-8")
            path.chmod(0o755)
        for p in control_dir.iterdir():
            if p.name == "control":
                p.chmod(0o644)

        tmp = out_dir / (name + ".part")
        cmd = ["dpkg-deb", "--root-owner-group", "-Zxz", "--build", str(pkg), str(tmp)]
        res = subprocess.run(cmd)
        if res.returncode != 0:
            # dpkg older than 1.19 has no --root-owner-group
            res = subprocess.run(["fakeroot"] + [c for c in cmd if c != "--root-owner-group"]
                                 if shutil.which("fakeroot") else
                                 [c for c in cmd if c != "--root-owner-group"])
        if res.returncode != 0:
            sys.exit("mbChess-Debian-Paket: dpkg-deb failed")
        tmp.replace(out_dir / name)
        say(f"{out_dir / name}")
        say(f"Depends: {', '.join(depends)}")
        say(f"install with:  sudo apt install {out_dir / name}")
    finally:
        if keep:
            say(f"package tree kept in {work}")
        else:
            shutil.rmtree(work, ignore_errors=True)


def png_size(path):
    with open(path, "rb") as f:
        head = f.read(24)
    if head[:8] != b"\x89PNG\r\n\x1a\n":
        return None
    w = int.from_bytes(head[16:20], "big")
    h = int.from_bytes(head[20:24], "big")
    return w if w == h and w in (16, 22, 24, 32, 48, 64, 128, 256, 512) else None


def main():
    ap = argparse.ArgumentParser(description="Builds a Debian package (.deb) of mbChess.")
    ap.add_argument("--build-dir", required=True, help="mbChess build folder (contains bin/)")
    ap.add_argument("--out", default=str(ROOT / "install" / "output"), help="output folder")
    ap.add_argument("--keep", action="store_true", help="keep the package tree (for checks)")
    ap.add_argument("--install-deps", action="store_true",
                    help="first install missing build packages (tools/install-build-deps.sh)")
    args = ap.parse_args()
    if args.install_deps and not install_deps():
        sys.exit("mbChess-Debian-Paket: build packages are missing, see above")
    build(version(), args.build_dir, Path(args.out), args.keep)


if __name__ == "__main__":
    main()
