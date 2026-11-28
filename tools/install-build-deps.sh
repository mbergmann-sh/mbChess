#!/bin/sh
# mbChess - installs everything that is needed to build mbChess, its engines and the
# Debian package on Debian 12/13, Ubuntu 24.04 and their derivatives.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Usage:
#   sh tools/install-build-deps.sh            lists the missing packages and asks
#   sh tools/install-build-deps.sh -y         installs without asking
#   sh tools/install-build-deps.sh --check    only lists, installs nothing (exit 1: missing)
#   sh tools/install-build-deps.sh --no-rust  without Rust (then Viridithas is not built)
#
# The packages come from apt (with sudo). Viridithas needs Rust 1.93 or newer, which
# the distributions do not have yet; the script then offers to install Rust for your
# user with rustup (https://rustup.rs, into ~/.cargo, no root needed).
#
# Users of the finished package need none of this: "sudo apt install ./mbchess_*.deb"
# installs the Qt libraries the program needs by itself.

set -u

YES=0
CHECK=0
RUST=1
for arg in "$@"; do
    case "$arg" in
        -y|--yes) YES=1 ;;
        --check) CHECK=1 ;;
        --no-rust) RUST=0 ;;
        -h|--help) sed -n '2,18p' "$0"; exit 0 ;;
        *) echo "install-build-deps: unknown option $arg (see --help)" >&2; exit 2 ;;
    esac
done

say() { echo "mbChess-Build-Pakete: $*"; }

if ! command -v apt-get >/dev/null 2>&1 || ! command -v dpkg-query >/dev/null 2>&1; then
    say "this script is for Debian/Ubuntu (apt). Install by hand: a C/C++ compiler, make,"
    say "Qt 6.4+ with qmake, Svg, SerialPort, Multimedia, Linguist tools, curl, git, python3."
    exit 1
fi

# Each line: what it is for, then the package names; the first one apt knows is used
# (the names differ between Debian 12, Debian 13 and Ubuntu).
NEEDS='
compiler|build-essential
qmake|qmake6
qt-base|qt6-base-dev
qt-tools|qt6-base-dev-tools
qt-svg|qt6-svg-dev libqt6svg6-dev
qt-serialport|qt6-serialport-dev libqt6serialport6-dev
qt-multimedia|qt6-multimedia-dev
qt-lrelease|qt6-l10n-tools qt6-tools-dev-tools
opengl|libgl-dev libgl1-mesa-dev
curl|curl
git|git
python3|python3
ca-certificates|ca-certificates
dpkg-deb|dpkg
dpkg-shlibdeps|dpkg-dev
fakeroot|fakeroot
strip|binutils
'

known() { apt-cache show "$1" >/dev/null 2>&1; }
installed() { [ "$(dpkg-query -W -f='${Status}' "$1" 2>/dev/null)" = "install ok installed" ]; }

# Prints the packages to install, "?<need>" for needs apt has no package for.
scan() {
    echo "$NEEDS" | while IFS='|' read -r what names; do
        [ -n "$what" ] || continue
        done_=0
        for n in $names; do
            if installed "$n"; then done_=1; break; fi
        done
        [ "$done_" -eq 1 ] && continue
        pick=""
        for n in $names; do
            if known "$n"; then pick="$n"; break; fi
        done
        if [ -n "$pick" ]; then echo "$pick"; else echo "?$what"; fi
    done
}

SUDO=""
[ "$(id -u)" -ne 0 ] && SUDO="sudo"

result=$(scan)
case "$result" in
    *\?*)
        # apt's package lists are missing or old (fresh system): update them first
        if [ "$CHECK" -eq 0 ]; then
            say "updating the package lists (apt update)"
            $SUDO apt-get update >/dev/null || say "apt update failed"
            result=$(scan)
        fi ;;
esac
missing=""
unknown=""
for p in $result; do
    case "$p" in
        \?*) unknown="$unknown ${p#?}" ;;
        *) missing="$missing $p" ;;
    esac
done

[ -n "$unknown" ] && say "apt has no package for:$unknown (run 'sudo apt update' first?)"

ask() {
    [ "$YES" -eq 1 ] && return 0
    printf '%s [J/n] ' "$1"
    read -r answer || return 1
    case "$answer" in n|N|nein|Nein|no|No) return 1 ;; *) return 0 ;; esac
}

status=0
if [ -z "$missing" ]; then
    say "all packages are installed"
else
    say "missing packages:$missing"
    if [ "$CHECK" -eq 1 ]; then
        status=1
    elif ask "Install them now with apt (sudo)?"; then
        $SUDO apt-get install -y $missing || { say "apt failed"; exit 1; }
    else
        say "not installed. By hand:  sudo apt install$missing"
        status=1
    fi
fi

# Rust for Viridithas (needs 1.93 or newer)
if [ "$RUST" -eq 1 ]; then
    [ -x "$HOME/.cargo/bin/cargo" ] && PATH="$HOME/.cargo/bin:$PATH"
    rust_ok=0
    if command -v cargo >/dev/null 2>&1 && command -v rustc >/dev/null 2>&1; then
        v=$(rustc --version | sed -n 's/^rustc \([0-9]*\)\.\([0-9]*\).*/\1 \2/p')
        set -- $v
        if [ "${1:-0}" -gt 1 ] || { [ "${1:-0}" -eq 1 ] && [ "${2:-0}" -ge 93 ]; }; then
            rust_ok=1
        fi
    fi
    if [ "$rust_ok" -eq 1 ]; then
        say "Rust $(rustc --version | cut -d' ' -f2) is fine for Viridithas"
    else
        if command -v rustc >/dev/null 2>&1; then
            say "Rust $(rustc --version | cut -d' ' -f2) is too old for Viridithas (needs 1.93+)"
        else
            say "no Rust: Viridithas needs Rust 1.93 or newer (the other engines do not)"
        fi
        if [ "$CHECK" -eq 1 ]; then
            status=1
        elif command -v rustup >/dev/null 2>&1; then
            if ask "Update Rust with rustup?"; then rustup update stable || status=1; else status=1; fi
        elif ask "Install Rust for your user with rustup (https://rustup.rs, into ~/.cargo)?"; then
            if [ -n "$SUDO" ] || [ -z "${SUDO_USER:-}" ]; then
                curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y || status=1
            else
                # run as root through sudo: install for the real user, not for root
                sudo -u "$SUDO_USER" sh -c "curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh -s -- -y" || status=1
            fi
            say "open a new terminal (or run '. ~/.cargo/env') so that qmake6 finds cargo"
        else
            say "without Rust mbChess is built without Viridithas"
        fi
    fi
fi

[ "$status" -eq 0 ] && say "ready. Build with:  qmake6 CONFIG+=installer <path>/mbChess.pro && make -j$(nproc 2>/dev/null || echo 4)"
exit "$status"
