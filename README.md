# mbChess

Freies Schachprogramm (GPL-3.0-or-later) in C++/Qt für Windows, Linux, macOS und später Android.
*Free chess GUI (GPL-3.0-or-later) written in C++/Qt for Windows, Linux, macOS and later Android.*

> **Stand: Phase 2, Teil 1.** Enthalten sind die Bibliotheken `chesscore` (Zuggenerator,
> FEN, SAN/UCI, Partiebaum mit Varianten), `pgn` (Lesen/Schreiben mit Varianten, Kommentaren,
> NAGs und `[%eval]`) und `engine` (UCI-Protokoll), Unit-Tests und das Programm: Brett in
> Holzoptik mit Maus- und Tastatureingabe, Partie gegen Stockfish oder jede andere UCI-Engine
> (Farbwahl Weiß/Schwarz/Zufall, Spielstärke, CPU-Kerne, Hash), Schachuhr mit Bedenkzeit und
> Inkrement, PGN laden und speichern, FEN setzen/kopieren, Zug zurücknehmen, Aufgeben,
> Deutsch/Englisch.
>
> Andockbare Fenster (Uhr, Engine, Notation, Bewertung; Menü „Ansicht“, Anordnung wird
> gespeichert). Die Notation zeigt Varianten und Kommentare; ein Klick auf einen Zug oder in den
> Bewertungsgraphen springt dorthin, Pfeiltasten/Pos1/Ende blättern. Ohne Engine und Uhr
> (Analyse, geladene PGN) wird ein Zug mitten in der Partie zur Variante; Kontextmenü: Variante
> hochstufen, ab hier löschen, Kommentar bearbeiten. In einer Partie gegen die Engine kann man
> zurückblättern, gezogen wird aber nur am Ende. Die Bewertungen der Engine werden an den Zügen
> gespeichert und als `[%eval]` in die PGN geschrieben. Die Notation ist eine Zugliste (ein Zug
> pro Zeile: Nummer, Weiß, Schwarz); Kommentare und Varianten stehen in eigenen Zeilen.
>
> Partiedatenbanken: Eine PGN-Datei mit mehreren Partien erscheint im Fenster „Partien“ (Liste mit
> Spielern, Elo, Ergebnis, Zügen, ECO, Datum, Turnier, Ort; sortierbar, Suchfeld). Doppelklick
> oder Enter lädt eine Partie, Strg+Bild↑/Bild↓ blättert durch die (gefilterte) Liste. Große Dateien
> (60.000 Partien, 16 MB in wenigen Sekunden) werden im Hintergrund eingelesen; die Partie selbst
> wird erst beim Öffnen gelesen.
>
> Engines: Stockfish, Arasan, Fruit, Toga II, Glaurung, GNU Chess, Viridithas, Texel, Patricia und die
> Tarrasch Toy Engine liegen im Quelltext in
> `engines/` und werden mitgebaut (Netze lädt der Build einmalig herunter). Crafty (XBoard)
> lädt der Build als Quelltext herunter und kompiliert es nur für den eigenen Gebrauch (Lizenz
> verbietet die Weitergabe, daher nicht im Installer). Das Programm sucht
> Engines nur im Ordner `engines` neben dem Programm; siehe [engines/README.md](engines/README.md).
> Menü „Engines → Engines verwalten“: weitere Engines hinzufügen und entfernen, umbenennen und
> die Optionen jeder Engine einstellen (werden bei jedem Start übergeben).
> „Neue Partie“ → Gegner „Engine gegen Engine“: zwei Engines spielen mit den übrigen
> Einstellungen (Stärke, Kerne, Hash, Bedenkzeit, Bücher) gegeneinander.
> Menü „Datei → Benchmarks → Engine-Turnier“: die ausgewählten Engines spielen ein Rundenturnier
> unter gleichen Bedingungen (Vorgabe 5 min + 3 s, jede Eröffnung mit beiden Farben); vorher und
> während des Laufs wird die erwartete Dauer angezeigt, Fortschritt und Abbruch in der
> Statusleiste, Tabelle (Sieger hervorgehoben) und alle Partien mit Dauer im Ergebnisfenster,
> Partien als PGN speicherbar. Wahlweise (Vorgabe an, auch im Menü umschaltbar) läuft die aktuelle
> Benchmark-Partie auf dem Brett mit, samt Spielernamen, Uhren, Engine-Ausgabe, Notation und
> Bewertungsgraph; eigene Züge sind dann gesperrt, zurückblättern geht.
> Menü „Engines → Engine-Analyse“ (Alt+F2, auch in der Werkzeugleiste): eine wählbare Engine
> („Analyse-Engine“) rechnet ohne Zeitlimit an der Stellung auf dem Brett und zeigt 1 bis 5
> Zeilen („Analyse-Zeilen“, MultiPV); sie folgt jedem Zug und jedem Blättern. Die Bewertung der
> besten Zeile wird am Zug gespeichert (Bewertungsgraph, PGN). Während einer laufenden Partie
> gegen Engine oder Uhr ist die Analyse gesperrt; eine beendete Partie wird zum Analysebrett.
> Menü „Partie → Stellung aufbauen“ (Strg+E, Werkzeugleiste): Brett-Editor mit Figurenleisten
> oben (Schwarz) und unten (Weiß) im eingestellten Figurensatz; Figuren per Drag & Drop oder
> Figur wählen und Felder anklicken; vom Brett ziehen oder Rechtsklick entfernt, Strg+Ziehen
> kopiert. Zugrecht Weiß/Schwarz/vom Programm bestimmt (wer im Schach steht, sonst Weiß),
> Rochaderechte, En passant, Zugnummer, FEN-Zeile, Laden/Speichern als .fen, laufende Prüfung
> der Stellung. Die übernommene Stellung steht als Analysebrett im Hauptfenster; „Neue Partie“
> bietet dann „Von der Stellung auf dem Brett aus spielen“.
> Menü „Engines → Matt suchen“ (Strg+M, Werkzeugleiste): für jede Stellung auf dem Brett; Matt in
> höchstens N Zügen, Engine wählbar (Vorgabe Stockfish, der `go mate N` bekommt), Zeitlimit,
> wahlweise Nebenlösungen (MultiPV). Die Lösung lässt sich als Variante einfügen. Läuft eine
> Partie, fragen Brett-Editor, Mattsuche und „Stellung aus FEN“ vorher nach.
> DGT-Brett (Menü „Brett“, Strg+D, Werkzeugleiste mit eigenem Symbol für an und aus): USB-Bretter und
> gekoppelte Bluetooth-Classic-Bretter über den seriellen Anschluss (9600 8N1). Der Dialog
> „DGT-Brett einstellen“ sucht beim Öffnen selbst (Trademark/Version/Stellung abfragen), zeigt die
> Anschlüsse zur Auswahl und hat „Neu suchen (Re-Scan)“; Ausrichtung automatisch an der Grundstellung.
> Züge auf dem Brett werden gespielt, sobald das Brett 300 ms ruhig ist und genau ein legaler Zug
> passt; der Zug der Engine steht in der Statusleiste („bitte Sf6 auf dem Brett ziehen“). „Stellung
> vom DGT-Brett übernehmen“ öffnet den Brett-Editor mit den Figuren des Bretts. DGT Pegasus
> (nur Bluetooth LE) wird nicht unterstützt. Beim Programmstart prüft mbChess erst den gespeicherten
> Anschluss, dann „blind“ alle anderen; danach erkennt es Ein- und Ausstecken (neue Anschlüsse) und
> Ein- und Ausschalten (Abfrage alle 4 s, gespeicherter Anschluss alle 6 s) und verbindet sich
> selbst wieder (abschaltbar im DGT-Dialog).
> Startbildschirm mit Foto, Ladeschritten und Fortschrittsbalken (Menü „Ansicht → Startbildschirm
> anzeigen“, Einstellung `ui/splash`).
> Menü „Engines → Endspiel-Datenbanken“: Syzygy-Tabellen mit 3, 4 und 5 Steinen
> (WDL etwa 378 MB, mit DTZ etwa 939 MB) herunterladen, mit Fortschritt, Abbruch und Fortsetzen;
> Ordner `Syzygy` neben dem Programm (änderbar). Stockfish, Arasan, Viridithas, Texel, Patricia und
> Crafty nutzen sie automatisch, im Benchmark nur auf Wunsch (Vorgabe aus, wegen der Fairness).
> Gaviota-Tabellen (`*.gtb.cp4`, Ordner `Gaviota`) nutzt Texel; mbChess zeigt damit in der Analyse
> den Matt-Abstand bei bis zu 5 Steinen (`src/libs/gaviota`).
> Ponder: im Dialog „Neue Partie“ wählbar, die Engine rechnet in der Bedenkzeit des Menschen weiter.
>
> Sprachaufnahme (`tools/voicerecorder`, Qt 6): wird mitgebaut, wenn sie noch nicht existiert,
> und ist im Windows-Installer optional wählbar.
>
> Themen (Menü „Brett“): Standard ist ein graues Brett mit Figuren auf Holzscheiben; dazu das
> gezeichnete Holzbrett „mbChess (klassisch)“, 12 Bretter und 9 Figurensätze aus AmiChess
> sowie 173 Brettthemen und 49 Figurensätze aus Lucas Chess, in Untermenüs. Zuggeräusche und
> Zugansagen (Stimme Deutsch oder Englisch, automatisch nach Programmsprache), beides abschaltbar.
> Eigene Stimmen nimmt das Programm `tools/voicerecorder` (Qt 6) Wort für Wort auf, mit
> Abhören und Neuaufnahme, samt einer Begrüßung beim Programmstart (`welcome.wav`); sie erscheinen
> unter „Brett → Stimme“, siehe [tools/voicerecorder/README.md](tools/voicerecorder/README.md).
> Fenster-Design (Menü „Ansicht“): System oder „Dunkles Design“ (aus AmigaED 4). Option
> „Immer in Dame umwandeln“ im Dialog „Neue Partie“. Dort auch das Eröffnungsbuch (PolyGlot `.bin`,
> eingebaut ist ein eigenes Buch aus den benannten Eröffnungen von lichess) mit einstellbarer Tiefe
> sowie Chessbooks: Partiedatenbanken (PGN), aus denen mbChess wie Crafty ein Buch mit den
> Ergebnissen der Partien baut und daraus gut abschneidende Züge spielt. Eröffnungsbücher liegen
> im Ordner `Openings`, Datenbanken im Ordner `Chessbooks` neben dem Programm. Dazu das engine-eigene Buch (Fruit, Toga, GNU Chess; Arasan mit eigener Datei). Die erkannte
> Eröffnung (ECO-Code und Name, bei deutscher Oberfläche mit deutschem Namen) steht rechts in der
> Statusleiste. Werkzeugleiste; Fenstergröße, Position, Fensteranordnung und Optionen werden laufend gespeichert.
> Eigene Themen im Ordner `themes` neben dem Programm, Aufbau siehe `src/app/themes.h`
> (Werkzeuge in `tools/`).
>
> Start mit einer bestimmten Stellung: `mbchess --fen "<FEN>"`, mit einer PGN-Datei: `mbchess partie.pgn`

## Voraussetzungen

| System | Qt | Compiler |
| --- | --- | --- |
| Windows 10/11 | Qt 6.4 oder neuer (entwickelt mit 6.12), Kit *MinGW 64-bit* | MinGW-w64 (über den Qt-Installer oder MSYS2) |
| Linux (Debian 12 und neuer) | Qt 6.4+ (`qt6-base-dev qt6-svg-dev qt6-multimedia-dev qt6-serialport-dev qt6-tools-dev-tools qt6-l10n-tools`) oder Qt 5.15 (`qtbase5-dev libqt5svg5-dev qtmultimedia5-dev libqt5serialport5-dev qttools5-dev-tools`) | gcc ≥ 9 oder clang |
| macOS ab 10.15 Catalina | Qt 5.15 (Catalina) oder Qt 6 (neuere macOS) | Xcode / clang |

C++17 ist erforderlich. Gebaut wird mit **qmake**.

**Qt Multimedia** ist optional und wird für Zuggeräusche und Zugansagen gebraucht. Unter Windows nachinstallieren
über das Qt-Wartungsprogramm (*Qt 6.x → Zusätzliche Bibliotheken → Qt Multimedia*). Fehlt es, baut
qmake mbChess ohne Klänge und meldet das.

**Qt Serial Port** ist optional und wird für DGT-Bretter gebraucht (Qt-Wartungsprogramm: *Qt 6.x →
Zusätzliche Bibliotheken → Qt Serial Port*). Fehlt es, baut qmake mbChess ohne DGT-Unterstützung und
meldet das; der Menüpunkt erklärt es dann.

## Bauen

Am besten in einem eigenen Build-Ordner:

```sh
mkdir build && cd build
qmake ../mbChess.pro
make -j8            # Windows/MinGW: mingw32-make -j8
make check          # Unit-Tests ausführen
```

Beim ersten Bauen werden auch die Engines kompiliert (einige Minuten) und ihre neuronalen
Netze heruntergeladen (etwa 125 MB, dafür wird `curl` benutzt). Ohne Engines: `qmake CONFIG+=no_engines ../mbChess.pro`.

Das Programm liegt danach in `build/bin/` (`mbchess`, `mbchess.exe` bzw. `mbChess.app`).
In Qt Creator einfach `mbChess.pro` öffnen und das passende Kit wählen.

### Windows-Installer

Nach einem Release-Build baut `install\make-installer.bat <Build-Ordner>` zwei Installer
(Deutsch und Englisch) mit dem Qt Installer Framework nach `install/output/`. Alternativ
als letzter Build-Schritt: `qmake CONFIG+=installer`. Einzelheiten in
[install/README.md](install/README.md).

## Aufbau

```
mbChess.pro          Hauptprojekt (TEMPLATE = subdirs)
common.pri           gemeinsame Einstellungen: C++17, Qt-Versionsprüfung, Ausgabeordner
src/libs/chesscore/  Regeln, Zuggenerator, FEN, SAN/UCI, Partie (ohne GUI)
src/libs/pgn/        PGN lesen und schreiben
src/libs/engine/     UCI-Engines über QProcess, Engine-Suche im Ordner engines
engines/             freie Engines im Quelltext mit Build-Schritten (engine.pri, <name>.pro)
src/libs/compat/     Hilfsfunktionen für Unterschiede zwischen Qt 5.15 und Qt 6
src/app/             Programm (Qt Widgets)
translations/        Deutsch/Englisch (Qt Linguist)
tests/               Unit-Tests (Qt Test)
docs/                Architektur-Notizen, Handbuch (docs/manual: PDF und CHM, Quelle in docs/manual/src)
install/             Windows-Installer (Qt Installer Framework)
```

Weitere Bibliotheken (`net`, `dgt`, `training`) kommen in den nächsten Phasen dazu, siehe [docs/ARCHITEKTUR.md](docs/ARCHITEKTUR.md).

## Übersetzungen

Quelltexte im Code sind englisch, die deutsche Übersetzung steht in `translations/mbchess_de.ts`.
Nach Änderungen an Texten im Code:

```sh
lupdate -no-obsolete src/app/app.pro     # .ts-Dateien aktualisieren
linguist translations/mbchess_de.ts      # übersetzen
```

Die `.qm`-Dateien werden beim Bauen automatisch erzeugt und ins Programm eingebettet.

## Lizenz

GNU General Public License, Version 3 oder neuer, siehe [LICENSE](LICENSE).
