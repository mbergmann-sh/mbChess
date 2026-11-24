# Engines

mbChess bringt freie UCI-Engines im Quelltext mit und baut sie zusammen mit dem Programm.
Jede Engine landet in einem eigenen Unterordner von `engines` neben dem Programm:

```
bin/mbchess(.exe)
bin/engines/stockfish/stockfish(.exe)
bin/engines/arasan/arasan(.exe)  + arasanv8-20261005.nnue
bin/engines/fruit/fruit(.exe)
bin/engines/toga/toga(.exe)
bin/engines/glaurung/glaurung(.exe)
bin/engines/gnuchess/gnuchess    + smallbook.bin, gnuchess.ini
bin/engines/viridithas/viridithas(.exe)
bin/engines/texel/texel(.exe)
bin/engines/patricia/patricia(.exe)
bin/engines/tarrasch/tarrasch(.exe)
bin/engines/crafty/crafty(.exe)  + book.bin   (Quelltext wird beim Bauen geladen, s. u.)
```

Unter macOS liegt der Ordner im Programmpaket: `mbChess.app/Contents/MacOS/engines/`.
mbChess sucht Engines **nur dort** (im Ordner `engines` und eine Ebene tiefer). Eine Engine an
einem anderen Ort lässt sich im Dialog „Neue Partie“ über „Andere Engine…“ wählen.

## Enthaltene Engines

| Engine | Version | Lizenz | Quelle (Stand) | Gebaut mit |
| --- | --- | --- | --- | --- |
| Stockfish | 19 | GPL-3.0-or-later | [official-stockfish/stockfish](https://github.com/official-stockfish/stockfish), Tag `sf_19` (edb0d9d) | `stockfish.pro` |
| Arasan | 26.1 | MIT | [jdart1/arasan-chess](https://github.com/jdart1/arasan-chess), Tag `v26.1` (0c0bebd), mit [Fathom](https://github.com/jdart1/Fathom) (c9c6fef, MIT) | `arasan.pro` |
| Fruit | 2.1 | GPL-2.0-or-later | [Warpten/Fruit-2.1](https://github.com/Warpten/Fruit-2.1) (09627df) | `fruit.pro` |
| Toga II | 1.2.1a (Mehrkern) | GPL-2.0-or-later | [zwegner/toga-mp](https://github.com/zwegner/toga-mp) (e329b97) | `toga.pro` |
| Glaurung | 2.2 | GPL-3.0-or-later | [phenri/glaurung](https://github.com/phenri/glaurung) (1ac8827), von Tord Romstad, Vorläufer von Stockfish | `glaurung.pro` |
| GNU Chess | 6.2.5e | GPL-3.0-or-later | [madnight/gnuchess](https://github.com/madnight/gnuchess) (f910dd4) | `gnuchess.pro` |
| Viridithas | 21.0.0 | AGPL-3.0 | [cosmobobak/viridithas](https://github.com/cosmobobak/viridithas) (4609ff2) | `viridithas.pro` (Rust) |
| Texel | 1.13a7 | GPL-3.0-or-later | [peterosterlund2/texel](https://github.com/peterosterlund2/texel) (b1883f8), von Peter Österlund; Netz `nndata.tbin.compr` (6 MB) im Quelltext, eingebettet | `texel.pro` |
| Patricia | 5.1 | MIT | [Adam-Kulju/Patricia](https://github.com/Adam-Kulju/Patricia) (8a19bab), sehr angriffslustig; drei kleine Netze (`engine/nets`, je 1,5 MB) eingebettet, Fathom für Syzygy | `patricia.pro` (C++20) |
| Tarrasch Toy Engine | 0.908 | MIT | [billforsternz/tarrasch-toy-engine](https://github.com/billforsternz/tarrasch-toy-engine) (96319e7), die kleine Demo-Engine der Tarrasch-GUI, schwach – gut für Anfänger | `tarrasch.pro` |

### Endspiel-Datenbanken (Syzygy und Gaviota)

Stockfish, Arasan, Viridithas, Texel, Patricia (Option `SyzygyPath`) und Crafty
(`tbpath`/`egtb`) lesen Syzygy-Tabellen. mbChess übergibt ihnen den Ordner aus
„Engines → Endspiel-Datenbanken“ (Vorgabe: `Syzygy` neben dem Programm), sobald dort Tabellen
liegen; der Dialog lädt die Tabellen mit 3–5 Steinen von tablebase.lichess.ovh oder
tablebase.sesse.net herunter. Fruit, Toga II, Glaurung, GNU Chess und Tarrasch spielen ohne Tabellen.

**Gaviota-Tabellen** (`*.gtb.cp4`, Abstand bis zum Matt) liest Texel (Option `GaviotaTbPath`).
mbChess selbst liest sie ebenfalls, mit dem Probecode aus
[michiguel/Gaviota-Tablebases](https://github.com/michiguel/Gaviota-Tablebases) (981472c, MIT) in
`src/libs/gaviota/gtb/` (unverändert, gebaut von `src/libs/gaviota/gaviota.pro` mit `-DZ_PREFIX`),
und zeigt in der Analyse bei Stellungen mit bis zu 5 Steinen das Ergebnis unter dem Brett
(„Weiß setzt in 9 Zügen matt“). Ordner: `Gaviota` neben dem Programm oder ein eigener; einen
Download bietet mbChess nicht an.

### Ponder

Gegen einen Menschen und mit voller Stärke kann eine UCI-Engine mit der Option `Ponder`
(Stockfish, Arasan, Texel, Fruit, Toga, Glaurung …) in der Bedenkzeit des Gegners weiterrechnen
(„Neue Partie“ → „Engine denkt in Ihrer Bedenkzeit mit (Ponder)“). mbChess schickt dann `go ponder` mit dem
erwarteten Zug und bei einem Treffer `ponderhit`, sonst `stop` und eine neue Suche.

### Crafty (nur zum eigenen Gebrauch)

**Crafty 25.2** von Robert Hyatt ([MartinMSPedersen/Crafty-Chess](https://github.com/MartinMSPedersen/Crafty-Chess))
darf laut Lizenz nur privat genutzt und **nicht weitergegeben** werden. Deshalb liegt sein
Quelltext *nicht* in mbChess: `crafty.pro` lädt ihn beim ersten `make` in den Build-Ordner
(`engines-src/crafty`, per `git clone`, sonst das GitHub-Archiv mit `curl` und `tar`) und
kompiliert `crafty.c` (ein einziges Übersetzungsmodul) mit `-DSYZYGY -DSKILL -DCPUS=64` nach
`bin/engines/crafty/`, zusammen mit dem Eröffnungsbuch `book.bin`. Der Windows-Installer lässt
den Ordner aus. Ohne Crafty bauen: `qmake "MBCHESS_ENGINES=stockfish arasan fruit toga gnuchess viridithas"`.

Crafty spricht das **XBoard-Protokoll** (CECP); mbChess unterstützt es in
`src/libs/engine/xboard.cpp`. Hash, Kerne und Spielstärke setzt mbChess wie bei UCI-Engines
(`memory`, `cores`, `skill` 1–100), „OwnBook“ schaltet `book on/off`.

Die Quellen sind unverändert übernommen (ohne `.git`, Testdaten, Grafiken und fertige
Programmdateien), mit wenigen kleinen Ausnahmen, jeweils mit `mbChess:` markiert:

- `arasan/src/types.h`: `_aligned_malloc` auch für MinGW (statt `std::aligned_alloc`).
- `arasan/src/globals.cpp`: POSIX-Header nur außerhalb von Windows.
- `glaurung/src/evaluate.cpp`: `init_safety()` las ein Element hinter `SafetyTable` (Absturz bei
  der ersten Suche mit heutigen Compilern).
- `glaurung/src/movepick.cpp`: `PhaseTable` beginnt mit `PH_STOP`, damit der Index der
  Hauptsuche nicht −1 ist.
- `tarrasch/src/mbchess_port.h` (neu): die wenigen Windows-Aufrufe der Tarrasch Toy Engine
  (`GetTickCount`, Semaphoren, `CreateThread`, `strcpy_s`, `fopen_s` …) für Linux und macOS;
  `toyengine.cpp`, `ChessEngine.cpp` und `thc.h` binden ihn statt `windows.h` ein.

Texel: übernommen sind `lib/texellib` und `app/texel` (ohne die Werkzeuge, Tests und das Netz-
Training), gebaut wie upstreams CMake-Projekt, das Netz wird mit `incbin` eingebettet
(`qmake/texel.pro` schreibt dazu `texel_nndata.cpp` in den Build-Ordner). Die `CMakeLists.txt`
von upstream liegen noch dabei, werden aber nicht benutzt. Patricia: `engine/src` und
`engine/nets`; `-Wa,-I<engine>` lässt den Assembler die Netze finden. Patricias Spielstärke
setzt mbChess über `Skill_Level` (1–20 für etwa 500–3000 Elo, 21 = volle Stärke).

Glaurung: `timeoday.cpp` (nur für MSVC) wird nicht mitgebaut, die fertigen Programme, das
Makefile und `polyglot.ini` aus dem Original sind nicht übernommen. Glaurung liest
PolyGlot-Bücher über die Option „Book File“; mbChess setzt dort sein eigenes Buch.

Für GNU Chess ersetzen `gnuchess-config/` (`config.h`, `configmake.h`, `GCint.h`) das sonst
nötige `configure`. `qmake/arasan_tbprobe.cpp` bindet Fathoms C-Code als C++ ein, wie es Arasans
Makefile tut.

## Bauen

Die Engines werden beim normalen Bauen von `mbChess.pro` mitgebaut. Aufbau:

- `engines/engines.pro`: wählt die Engines aus.
- `engines/<name>.pro`: kleine Hüllen-Projekte (`wrapper.pri`), die qmake und make für die
  eigentliche Engine aufrufen. So bietet Qt Creator die Engines nicht als startbare Programme an.
- `engines/qmake/<name>.pro`: die eigentlichen Projekte der Engines, gemeinsame Einstellungen
  in `engine.pri`.
- `stockfish_net.pro` und `viridithas.pro`: Download der Netze bzw. Bau mit `cargo`.

```sh
qmake ../mbChess.pro
make -j8                  # Windows: mingw32-make -j8
```

- Gebaut wird mit demselben Compiler wie mbChess (MinGW, gcc oder clang), optimiert mit `-O3`.
  Unter Windows werden die Engines statisch gelinkt, sie brauchen keine MinGW-DLLs.
- `make` baut nur, was fehlt oder sich geändert hat. Fertige Engines bleiben unangetastet.
- Neuronale Netze, die für das Repository zu groß sind, lädt der Build **einmalig** mit `curl`
  herunter (in Windows 10/11, Linux und macOS enthalten):
  - Stockfish: `nn-1a298aa575a0.nnue` (etwa 100 MB) in den Build-Ordner, wird ins Programm eingebettet.
  - Arasan: `arasanv8-20261005.nnue` (etwa 25 MB) neben das Programm.
  - Viridithas: das neueste Netz aus [viridithas-networks](https://github.com/cosmobobak/viridithas-networks)
    (CC0) nach `engines/viridithas/viridithas.nnue.zst`, wird eingebettet.
  Schlägt ein Download fehl, bricht der Build mit einer Meldung ab; der nächste `make` versucht
  es erneut. Man kann die Datei auch von Hand an die genannte Stelle legen.

### Auswahl und Optionen

| Aufruf | Wirkung |
| --- | --- |
| `qmake CONFIG+=no_engines ../mbChess.pro` | Engines gar nicht bauen |
| `qmake "MBCHESS_ENGINES=stockfish fruit" ../mbChess.pro` | nur diese Engines bauen |
| `qmake MBCHESS_ENGINE_CPU=sse41 ../mbChess.pro` | für ältere x86-CPUs ohne AVX2 (vor etwa 2015) |
| `qmake MBCHESS_ENGINE_CPU=generic ../mbChess.pro` | ohne besondere CPU-Befehle |

Standard ist AVX2 auf x86-64 und NEON auf ARM64 (Apple Silicon).

### Besonderheiten je Plattform

- **GNU Chess** braucht POSIX-Pipes und `select()` und wird deshalb unter Windows übersprungen.
  Linux und macOS: wird gebaut. mbChess startet es mit `--uci`.
- **Viridithas** ist in Rust geschrieben und wird nur gebaut, wenn `cargo` gefunden wird
  (Rust 1.93 oder neuer, <https://rustup.rs>). Unter Windows mit MinGW passt die Toolchain
  `x86_64-pc-windows-gnu` (`rustup default stable-x86_64-pc-windows-gnu` oder MSYS2:
  `pacman -S mingw-w64-x86_64-rust`).
- **Spielstärke-Stufen** (Elo) wirken bei Engines mit `UCI_LimitStrength` (Stockfish, Arasan,
  Texel), `Skill Level` oder `Skill_Level` (Patricia). Fruit, Toga, GNU Chess und Tarrasch
  spielen immer mit voller Stärke.

## Engines aktualisieren

Neue Quellen in den jeweiligen Ordner kopieren und in `qmake/<engine>.pro` die Dateiliste und, bei
Stockfish und Arasan, den Namen des Netzes anpassen (Stockfish: `EvalFileDefaultName` in
`stockfish/src/evaluate.h`, Arasan: `NETWORK` in `arasan/src/Makefile`).
