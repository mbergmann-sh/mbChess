# Viridithas by Cosmo Bobak (AGPL-3.0), written in Rust, sources in viridithas/
#
# Needs Rust 1.93 or newer (https://rustup.rs). engines.pro skips this engine
# when "cargo" is not found. The neural network (CC0) is not part of the
# repository: it is downloaded once from the latest release of
# https://github.com/cosmobobak/viridithas-networks into viridithas/viridithas.nnue.zst,
# where Viridithas' build script expects it, and embedded into the program.
include(engine.pri)

TEMPLATE = aux
QMAKE_EXTRA_TARGETS += first
mbchessEngineDir(viridithas)
mkpath($$DESTDIR)

VIRI     = $$PWD/viridithas
VIRI_NET = $$VIRI/viridithas.nnue.zst
VIRI_API = https://api.github.com/repos/cosmobobak/viridithas-networks/releases/latest
win32 {
    VIRI_EXE = viridithas.exe
    viri_net.commands = powershell -NoProfile -Command \
        \"Invoke-WebRequest -OutFile '$$shell_path($${VIRI_NET}.part)' -Uri (Invoke-RestMethod $$VIRI_API).assets[0].browser_download_url\" \
        && $(MOVE) $$shell_quote($$shell_path($${VIRI_NET}.part)) $$shell_quote($$shell_path($$VIRI_NET))
} else {
    VIRI_EXE = viridithas
    viri_net.commands = curl -sL --fail $$VIRI_API | grep -o \'\"browser_download_url\": *\"[^\"]*\' | head -n 1 \
        | sed \'s/.*\"//\' | xargs curl -L --fail -o $$shell_quote($${VIRI_NET}.part) \
        && $(MOVE) $$shell_quote($${VIRI_NET}.part) $$shell_quote($$VIRI_NET)
}
viri_net.target = $$VIRI_NET

equals(MBCHESS_ENGINE_CPU, avx2): VIRI_CPU = x86-64-v3
else: equals(MBCHESS_ENGINE_CPU, sse41): VIRI_CPU = x86-64-v2
else: VIRI_CPU =
!isEmpty(VIRI_CPU): VIRI_RUSTC_ARGS = -- -C target-cpu=$$VIRI_CPU

VIRI_TARGET_DIR = $$OUT_PWD/obj/viridithas
viri_exe.target   = $$DESTDIR/$$VIRI_EXE
viri_exe.depends  = $$VIRI_NET
viri_exe.commands = +cargo rustc --release --manifest-path $$shell_quote($$shell_path($$VIRI/Cargo.toml)) \
                        --target-dir $$shell_quote($$shell_path($$VIRI_TARGET_DIR)) $$VIRI_RUSTC_ARGS \
                    && $(COPY_FILE) $$shell_quote($$shell_path($$VIRI_TARGET_DIR/release/$$VIRI_EXE)) \
                                    $$shell_quote($$shell_path($$DESTDIR/$$VIRI_EXE))

first.depends += $$viri_exe.target
QMAKE_EXTRA_TARGETS += viri_net viri_exe
