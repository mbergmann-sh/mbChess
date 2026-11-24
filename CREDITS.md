# Credits / Herkunft von Grafiken und Fremdmaterial

| Material | Urheber | Lizenz | Quelle |
| --- | --- | --- | --- |
| Schachfiguren „cburnett“ (`src/app/resources/pieces/cburnett/`) | Colin M.L. Burnett | GPL-2.0-or-later | [lichess-org/lila](https://github.com/lichess-org/lila/tree/master/public/piece/cburnett) |
| Brett (Holzoptik, „mbChess (klassisch)“), graues Standardbrett, Holzscheiben der Standardfiguren | mbChess, im Programm gezeichnet; Symbole auf den Scheiben: „cburnett“ | GPL-3.0-or-later | – |
| Fenster-Design „Dunkles Design“ (`src/app/guitheme.cpp`) | Michael Bergmann, aus AmigaED 4 („Visual Studio Code Dark“, Farben von VS Code „Dark+“) | GPL | [AmigaED4-IDE](https://github.com/mbergmann-sh/AmigaED4-IDE) |
| Bretter und Figurensätze „AmiChess“, englische Original-Ansagen (`src/app/resources/themes/{boards,pieces}/<Name>`, `sounds/amichess`) | AmiChess (Amiga), umgewandelt mit `tools/amiga2themes.py` | GPL | AmiChess |
| Figurensätze und Brettthemen „Lucas Chess“ (`themes/pieces/lucas-*`, `themes/boards/lucas-*`) | Lucas Chess und seine Mitwirkenden; je Satz `author.txt`/`license.txt`, falls vorhanden (u. a. Ben Citak: GPL-2.0; Maurizio Monge: LGPL-2.0; Marc Graziani: GPL-2.0; Jin: GPL-2.0; Leipzig: GPL-3.0); übernommen mit `tools/lucas2themes.py` | GPL | [Lucas Chess](https://lucaschess.pythonanywhere.com) |
| Deutsche Zugansagen (`sounds/thorsten-de`) | Stimme „Thorsten“ von Thorsten Müller, erzeugt mit Piper (`tools/make-speech.py`, `tools/clean-sounds.py`) | CC0-1.0 (Stimme) | [Thorsten-Voice](https://github.com/thorstenMueller/Thorsten-Voice), [Piper](https://github.com/rhasspy/piper) |
| Englische Zugansagen (`sounds/libritts-en`) | Piper-Stimme „LibriTTS“ (Sprecher 100), erzeugt mit `tools/make-speech.py` | CC BY 4.0 (LibriTTS) | [LibriTTS](http://www.openslr.org/60/) |
| Eröffnungsnamen (`src/app/resources/openings/*.tsv`) und daraus erzeugtes Eröffnungsbuch `books/mbchess.bin` (`tools/make-opening-book.py`); deutsche Namen `names_de.tsv` von mbChess (`tools/translate-openings.py`) | lichess.org, chess-openings | CC0-1.0 | [lichess-org/chess-openings](https://github.com/lichess-org/chess-openings) |
| Mattaufgaben „lichess:“ (`src/app/resources/training/mate{1,2,3}.txt`), ausgewählt und mit dem Matt-Löser von mbChess geprüft (`tools/make-mate-puzzles.py`) | lichess.org (Lichess-Aufgabendatenbank, erzeugt aus Lichess-Partien) | CC0-1.0 | [database.lichess.org](https://database.lichess.org/#puzzles) |
| Mattaufgaben „mbchess:“ in denselben Dateien | mbChess | GPL-3.0-or-later | – |
| PolyGlot-Zufallszahlen (`src/libs/book/polyglotkeys.inc`) | Fabien Letouzey (PolyGlot, Fruit 2.1) | GPL-2.0-or-later | `engines/fruit/src/random.cpp` |
| Zuggeräusche (`sounds/effects`) | mbChess, synthetisch erzeugt (`tools/make-effects.py`) | GPL-3.0-or-later | – |
| Symbole der Werkzeugleiste (`src/app/resources/icons/toolbar`) | Michael Bergmann / mbChess | GPL-3.0-or-later | – |
| Programm-Icon (`src/app/resources/icons/`) | Michael Bergmann, Springer nach „cburnett“ von Colin M.L. Burnett | GPL-2.0-or-later | – |

## Engines (`engines/`, Einzelheiten in [engines/README.md](engines/README.md))

| Engine | Urheber | Lizenz |
| --- | --- | --- |
| Stockfish 19 | The Stockfish developers (siehe `engines/stockfish/AUTHORS`) | GPL-3.0-or-later |
| Arasan 26.1 | Jon Dart | MIT |
| Fathom (Syzygy-Tablebases in Arasan) | Ronald de Man, basil00, Jon Dart | MIT |
| Fruit 2.1 | Fabien Letouzey | GPL-2.0-or-later |
| Toga II | Thomas Gaksch, Zach Wegner u. a., auf Basis von Fruit | GPL-2.0-or-later |
| Glaurung 2.2 | Tord Romstad | GPL-3.0-or-later |
| GNU Chess 6.2.5 | Free Software Foundation | GPL-3.0-or-later |
| Viridithas 21 | Cosmo Bobak | AGPL-3.0; Netze CC0-1.0 |
| Texel 1.13a7 (mit Netz `nndata.tbin.compr` und Gaviota-Probecode) | Peter Österlund | GPL-3.0-or-later |
| Patricia 5.1 (mit Fathom für Syzygy) | Adam Kulju | MIT |
| Tarrasch Toy Engine 0.908 | Bill Forster | MIT |
| Crafty 25.2 (nicht enthalten, beim Bauen geladen) | Robert Hyatt | nur privater Gebrauch, keine Weitergabe |
| Gaviota-Tablebases, Probecode (`src/libs/gaviota/gtb/`, mit LZMA, zlib, LZF, Huffman) | Miguel A. Ballicora (LZMA: Igor Pavlov, zlib: Jean-loup Gailly und Mark Adler, LZF: Marc Lehmann) | MIT (LZMA: Public Domain, zlib: zlib-Lizenz, LZF: BSD-2-Clause) |
| Syzygy-Endspiel-Datenbanken (nicht enthalten, lädt der Benutzer im Programm herunter, Server lichess.org bzw. sesse.net) | Ronald de Man (Format und Generator) | frei verfügbar |

## About-Bild / About picture

`src/app/resources/images/about.jpg` (Ausschnitt, auch als Installer-Bild `install/images/watermark.png`): © Michael Bergmann 2026, für mbChess bereitgestellt.
