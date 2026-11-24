# Handbuch / Manual

Das Benutzerhandbuch in zwei Sprachen, jeweils als PDF und als Windows-Hilfe (CHM):

- `mbChess-Handbuch.pdf`, `mbChess-Handbuch.chm` (Deutsch)
- `mbChess-Manual.pdf`, `mbChess-Manual.chm` (English)

Die Quelle ist Markdown in `src/de/handbuch.md` und `src/en/manual.md`, die Bildschirmfotos liegen
in `src/<Sprache>/images/`. Version und Versionsname kommen aus `common.pri`. Neu bauen (Linux):

```sh
python3 tools/build-manual.py            # beide Sprachen, PDF und CHM
python3 tools/build-manual.py --no-pdf de
```

Dafür nötig: `pandoc`, `chmcmd` (Debian/Ubuntu: Paket `fp-utils`) und Node.js mit Playwright
(Chromium) für das PDF.

Der Build kopiert die Dateien nach `bin/Handbuch/` bzw. `bin/Manual/` neben das Programm (die
CHM nur unter Windows). Der Windows-Installer legt die CHM der Installer-Sprache immer mit dem
Programm ab (dazu eine Verknüpfung „mbChess-Hilfe“ im Startmenü), das PDF ist die optionale
Komponente „Handbuch (PDF)“.

Menü „Hilfe → Handbuch“ (F1) öffnet unter Windows die CHM, sonst das PDF im PDF-Betrachter des
Systems.
