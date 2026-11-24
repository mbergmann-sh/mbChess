# mbChess – Architektur (Kurzfassung)

Stand: 24.11.2026. Die ausführliche Planung liegt im Projektplan; hier stehen die Regeln,
die beim Programmieren gelten.

## Schichten

1. **Oberfläche** (`src/app`, Qt Widgets): Brett, Notation, Uhr, Engine-Fenster,
   Bewertungsprofil, Datenbank, Training/Kurs, Einstellungen. Andockbare Fenster (`QDockWidget`).
2. **GameManager**: verwaltet Partie, Uhr, Spielmodus und Angebote (Remis, Rücknahme, Aufgabe).
   Brett, Fenster und Gegner reden nie direkt miteinander (Vorbild: KDE Knights).
3. **Spieler-Protokolle** (je Farbe eins): Lokal, DGT-Brett, UCI-Engine, XBoard-Engine, FICS, WLAN-Partner.
4. **Bibliotheken** (statisch, ohne GUI-Code, mit Unit-Tests): `chesscore`, `pgn`, `book`,
   `engine`, `gaviota`, `net`, `dgt`, `training`.
5. **Plattform** (Qt): QProcess, QSerialPort bzw. JNI unter Android, QTcpSocket/QUdpSocket,
   Qt SQL (SQLite), QtMultimedia (nur `QAudioSink` bzw. unter Qt 5 `QAudioOutput`: ein Audiostrom, Wörter im Speicher), Qt Linguist.

## Regeln

- Bibliotheken unter `src/libs` enthalten keine GUI-Klassen (nur `QT = core` bzw. `network`, `sql`).
- Unterstützt werden **Qt 5.15** und **Qt 6.4+**. Versionsabhängiger Code steht nur in
  `src/libs/compat/compat.h`, nirgends sonst ein `#if QT_VERSION`.
- Kein QML (Android bekommt Touch-Layouts mit Qt Widgets).
- Texte im Code auf Englisch mit `tr()`, Übersetzung nach Deutsch in `translations/`.
- Notation intern immer englisches SAN (PGN-Standard); deutsche Figurenbuchstaben nur in der
  Anzeige über `mbc::localizeSan()`.
- Engine-Pfade nur über die Engine-Verwaltung, nie fest im Code.
- Hardware nur über Schnittstellen (`DgtTransport`), damit Android einen eigenen Transport bekommt.

## Phasen

| Phase | Inhalt |
| --- | --- |
| 0 | Grundgerüst, `chesscore` mit Perft-Tests, FEN/SAN, CI (**erledigt**) |
| 1 | Brett-Widget, Mensch gegen Mensch, Mensch gegen Stockfish, Uhr, PGN laden/speichern, DE/EN (**erledigt**) |
| 2 | Farbwahl, andockbare Fenster, Notation mit Varianten (Zugliste), Bewertungsprofil, PGN-Datenbank (Partienliste), .bin-Bücher und Chessbooks, Engine-Verwaltung (**erledigt**) |
| 3 | Engines bauen (**erledigt**), Installer Windows (**erledigt**, noch ungetestet), Engine-Analyse (**erledigt**), Engine gegen Engine (**erledigt**), Engine-Benchmark/Turnier (**erledigt**), Brett-Editor und Mattsuche (**erledigt**), .deb, .dmg → Release 0.5 |
| 4 | DGT-Brett (**USB/Bluetooth Classic erledigt**, Android offen), FICS, WLAN |
| 5 | Training und Grundkurs → Release 1.0 |
| 6 | Android (Lenovo TB125FU, moto g67) |

## Spielen und Analysieren

- **Partie** (gegen eine Engine oder mit Uhr): gezogen wird nur am Ende der Partie; man kann
  zurückblättern, die Engine zieht trotzdem weiter am Ende (`m_searchNode`).
- **Analyse** (ohne Engine und Uhr, z. B. geladene PGN): ein Zug an beliebiger Stelle wird zur
  Variante (`Game::addMove`). Das Partieende (Matt, Remis, Ergebnis) gilt nur für das Ende der
  Hauptvariante.
- Farbwahl: Weiß, Schwarz oder Zufall im Dialog „Neue Partie“. Im späteren Turniermodus werden
  die Farben nach den offiziellen Regeln (FIDE-Paarungsregeln) vergeben.

## Training

Menü „Training“ (`src/app/trainer.*`, Panel `trainingpanel.*` als Dock neben „Engine“): Aufgaben
laufen als normale Partie auf dem Hauptbrett. `Trainer::prepareMate()`/`prepareEndgame()` liefern
`GameSettings` und Startstellung, `MainWindow::startTraining()` startet sie, `Trainer::started()`
merkt sich `GameManager::gameSerial()`: solange die Nummer gleich ist, gehört die Partie zur Aufgabe.

- **Matt finden**: Aufgaben in `resources/training/mate<N>.txt` (`id;FEN;Gegnerzug;Lösung;Wertung`,
  Lichess-Format: FEN vor dem Gegnerzug), dazu `Training/mate<N>*.txt` und Lichess-CSV-Dateien
  (Thema `mateIn<N>`) neben dem Programm. Der Verteidiger ist `GameSettings::BuiltinTrainer`: der
  `GameManager` fragt `Trainer::defenderMove()`, der mit `mbc::MateSolver` (chesscore, vollbreite
  Suche mit Transpositionstabelle, Matt in 3 unter 0,1 s) die zäheste Verteidigung wählt. Jeder
  Zug des Benutzers geht vorher durch `Trainer::userMove()`; ein Zug, nach dem kein Matt in der
  Restzahl mehr erzwungen ist, wird abgewiesen. Fortschritt in `training/mate<N>/solved` (IDs,
  nur ohne Fehler und ohne Lösung gelöste).
- **Endspiele**: Zufallsstellungen (`Trainer::generate()`), gegen eine Engine (Texel bevorzugt,
  1 s je Zug). Prüfung nach jedem Benutzerzug: Gaviota (falls geladen) bzw. `mbc::kpk`
  (chesscore, KPK-Tabelle per Rückwärtsanalyse, 0,5 MB, beim ersten Gebrauch berechnet).

`tools/puzzletool` (nicht im normalen Build) prüft Aufgabendateien mit dem Matt-Löser;
`tools/make-mate-puzzles.py` wählt aus der Lichess-Datenbank aus.

## Engines

Mitgeliefert und aus dem Quelltext gebaut (`engines/`, siehe [engines/README.md](../engines/README.md)):
Stockfish (GPL-3), Arasan (MIT), Toga II (GPL-2+), Fruit 2.1 (GPL-2+), Glaurung 2.2 (GPL-3), GNU Chess (GPL-3, nicht unter
Windows), Viridithas (AGPL-3, nur mit Rust), Texel (GPL-3), Patricia (MIT, C++20), Tarrasch Toy
Engine (MIT, `mbchess_port.h` ersetzt die Windows-Aufrufe). Crafty (XBoard-Protokoll, `xboard.cpp` in
der Engine-Bibliothek) wird nicht mitgeliefert: der Build lädt den Quelltext und kompiliert ihn
nur für den eigenen Gebrauch, der Installer lässt ihn aus (Lizenz erlaubt keine Weitergabe).

Ponder (`GameSettings::ponder`, nur Mensch gegen Engine mit voller Stärke, UCI-Engines mit
Option `Ponder`): nach dem Engine-Zug startet `GameManager::startPondering()` ein `go ponder`
auf den erwarteten Zug (aus `bestmove … ponder`, sonst dem zweiten Zug der Hauptvariante,
`m_ponderNode`/`m_ponderMove`). `startEngineSearch()` schickt bei einem Treffer `ponderhit`
(`UciEngine::ponderHit()`), sonst wird die Ponder-Suche verworfen und neu gesucht. Während
des Ponderns ist `m_searchNode` −1: Info-Zeilen und ein `bestmove` der Ponder-Suche gehen ins
Leere, „Sofort ziehen“ betrifft sie nicht.

Endspiel-Datenbanken (`src/app/tablebases.*`): Syzygy-Ordner für die Engines (`SyzygyPath`),
Gaviota-Ordner für Texel (`GaviotaTbPath`) und für mbChess selbst: `src/libs/gaviota`
(Probecode von Miguel A. Ballicora in `gtb/`, C, MIT; `gaviotatb.h` als C++-Schicht mit
`mbc::Position`, eine globale Tabellenmenge hinter einem Mutex). Die Statuszeile zeigt in der
Analyse das Ergebnis bei bis zu 5 Steinen.

Engine-Benchmark (`src/app/benchmark.*`, `benchmarkdialog.*`, `benchmarkwindow.*`): `BenchmarkRunner`
spielt ein Rundenturnier (Kreismethode, 12 feste Eröffnungen, jede mit beiden Farben), je Partie
zwei frisch gestartete Engines mit gleichen Bedingungen (Zeit, Threads, Hash, kein eigenes Buch,
optional Elo-Begrenzung), eigene Uhr mit Zeitüberschreitung, Ende nach Regeln oder nach 500 Halbzügen.
Zuschauen: der Runner meldet `positionChanged` und `searchInfo`, liefert `currentGame()` (mit
Bewertungen an den Zügen), `clockMs()` und `thinkingSide()`. `MainWindow` (`m_watching`) setzt den
`GameManager` auf `setSuspended(true)`, kopiert die Partie nach `m_game`, stellt die eigene Uhr auf die Restzeiten, sperrt Brett und Eingabe und
beendet dafür die eigene Partie; eine eigene neue Partie oder das Laden einer Partie schaltet das
Zuschauen ab. Zwischen zwei Partien wartet der Runner dann 3 s statt 0,3 s.
Engine gegen Engine im Hauptfenster: der `GameManager` hat eine zweite `UciEngine` für Schwarz,
`engineFor(Farbe)` wählt die Engine, `GameSettings::EngineVsEngine` mit `engine2Path`.

GameManager (`src/app/gamemanager.*`, seit 0.1.11): hält die Partie (`game()`), die Einstellungen,
beide Engines, die Uhr, Eröffnungsbuch und Chessbook und führt den Spielablauf (wer am Zug ist,
Engine-Suche, Buchzüge, Partieende, Rücknahme, Aufgabe, ungesicherte Daten). Er kennt kein
Fenster: `MainWindow` hört auf seine Signale (`changed`, `engineOutputChanged`, `movePlayed`,
`gameEnded`, `playerNamesChanged`, `engineError`, `statusMessage`, `chessbookProgress`) und ruft
seine Funktionen auf (`startGame`, `loadGame`, `usePosition`, `applyMove`, `takeBack`, …).
`MainWindow::m_game` ist nur ein kurzer Name (Referenz) für `m_gm->game()`.

Engine-Analyse (`src/app/engineanalysis.*`): eigene `UciEngine` mit `go infinite`, MultiPV
(Einstellung `analysis/lines`), Engine aus `analysis/engine`. `MainWindow::updateAnalysis()` läuft
am Ende von `updateView()` und übergibt Startstellung + Züge bis zum angezeigten Knoten; gleiche
Stellung = keine neue Suche. Ab Tiefe 10 wird die Bewertung an den Knoten geschrieben, wenn dort
keine tiefere steht (Bewertungen aus PGN ohne Tiefe bleiben). `analysisAllowed()` sperrt die
Analyse in laufenden Partien und beim Zuschauen im Benchmark.

Brett-Editor (`src/app/boardeditordialog.*`): `BoardWidget::setEditMode(true)` schaltet die
Zugeingabe ab; Figuren werden mit `QDrag` (MIME `application/x-mbchess-piece`: „Figur [Quellfeld]“)
zwischen Brett und `PiecePalette` bewegt, das Brett meldet `positionEdited`. `Position` hat dafür
`setPiece`, `setSideToMove`, `setCastlingRights` (gekappt auf `possibleCastlingRights()`),
`setEnPassantSquare` (nur passende Felder), `setMoveCounters`, `isValid`; `inCheck()` verträgt
eine Stellung ohne König. Die übernommene Stellung startet über `GameManager::usePosition()` als
Analysebrett (`m_positionSetUp`: „Neue Partie“ bietet „von der Stellung auf dem Brett“ angehakt an).

Mattsuche (`src/app/matesearchdialog.*`): eigene `UciEngine`, MultiPV 4 für Nebenlösungen.
Stockfish bekommt `go mate N` (`GoParameters::mate`) und hört selbst auf; andere Engines suchen mit
`go infinite`, bis Zeile 1 ein Matt ≤ N meldet, dann noch bis Tiefe +4 oder 3 s (kürzeres Matt,
Zeilen festigen sich). Beendet sich die Engine, werden ihre `info string ERROR`-Zeilen gezeigt.
`MainWindow::confirmEndGame()` fragt vor Editor, Mattsuche und FEN nach, wenn eine Partie läuft;
die Mattsuche macht die Partie zum Analysebrett (`toAnalysisBoard()`).

DGT-Brett (`src/libs/dgt`, nur mit Qt Serial Port; sonst `MBCHESS_HAVE_DGT` nicht gesetzt):
`dgtprotocol.*` nach dem DGT-Protokollpapier 1.2.1 (Befehle, Nachrichten, Figurencodes, Feld 0 = a8,
Empfänger als Zustandsautomat), `dgt::Board` (QSerialPort 9600 8N1; Reset, Trademark, Version,
Board-Dump, Update-Modus; Stellung gilt 300 ms nach dem letzten Feld-Update als ruhig;
Auto-Drehung bei verdrehter Grundstellung), `dgt::Scanner` (Ports, VID 045B und Bluetooth zuerst,
Probe im Thread mit blockierendem QSerialPort; auch Wiederverbinden läuft nur über die Probe,
damit ein hängendes Bluetooth-`open()` die Oberfläche nie blockiert). Keep-alive: `Board` fragt alle
4 s die Seriennummer ab; ohne jede Antwort nach drei Anfragen gilt die Verbindung als verloren.
`MainWindow::dgtWatch()` (Timer 2 s) probt neu erschienene Anschlüsse und, wenn das Brett an
sein soll, alle 6 s den gespeicherten. `dgtStartupCheck()` läuft in `main()`; der Startbildschirm
(`splashscreen.*`, `startup::progress()`) wartet darauf höchstens 4 s. `MainWindow::onDgtBoardChanged()` spielt den Zug,
dessen Stellung genau der Brettstellung entspricht, `updateDgtStatus()` zeigt Übereinstimmung,
den auszuführenden Gegnerzug oder die Zahl abweichender Felder. Einstellungen `dgt/enabled`,
`dgt/port`, `dgt/autoScan`, `dgt/orientation`. Test ohne Brett: Pseudo-Terminal als Brett-Simulator
(Python `os.openpty`), Port-Name direkt im Dialog/Einstellung.

Endspiel-Datenbanken (`src/app/tablebases.*`, `tablebasedialog.*`): Einstellungen
(`tablebases/use`, `tablebases/folder`, Vorgabe `datafolders::syzygyFolder()`), Liste der 145
Tabellen mit 3–5 Steinen, Bestandsprüfung. `applyToEngine()` setzt beim `optionsKnown` die
UCI-Option `SyzygyPath`, falls die Engine sie hat; für Crafty bietet `xboard.cpp` eine virtuelle
Option `SyzygyPath`, die `tbpath <Ordner>` und `egtb` sendet. Der Pfad steckt in der
Buch-Signatur des Hauptfensters, eine Änderung startet die Engine also neu. Der Downloader
(`QNetworkAccessManager`, daher `QT += network`) lädt Datei für Datei als `.part`, prüft die
Magic Bytes (WDL `71 e8 23 5d`, DTZ `d7 66 0c a5`), benennt dann um und versucht bei Fehlern die
anderen Server; vorhandene Dateien werden übersprungen. Ein eigener Spiegel lässt sich mit der
Umgebungsvariable `MBCHESS_SYZYGY_URL` voranstellen.

Engine-Verwaltung (Menü „Engines“, `src/app/enginemanager.*`, `enginedialog.*`): Liste aus den
Engines im Ordner plus vom Benutzer hinzugefügten, eigener Anzeigename und die UCI-Optionen der
Engine (von der Engine erfragt, nur geänderte Werte gespeichert, beim Start vor dem ersten
`isready` gesetzt). Spielstärke, Kerne, Hash und Bücher setzt weiterhin mbChess selbst.

Das Programm sucht Engines nur im Ordner `engines` neben dem Programm (eine Ebene tief,
`mbc::engine::findEngines()`); andere Engines wählt der Benutzer von Hand.
