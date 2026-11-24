# Windows-Installer

mbChess wird unter Windows mit dem **Qt Installer Framework** (QtIFW 4.x) verteilt.
Es entstehen zwei Installer, einer auf Deutsch und einer auf Englisch:

```
install/output/mbChess-<Version>-Setup-de.exe
install/output/mbChess-<Version>-Setup-en.exe
```

## Voraussetzungen

- ein Release-Build von mbChess (Qt Creator, Kit „Qt 6.x MinGW 64-bit“), am besten mit Engines
- das Qt Installer Framework: Qt Maintenance Tool → *Developer and Designer Tools* →
  *Qt Installer Framework*. Das Skript findet es unter `C:\Qt\Tools\QtInstallerFramework\<Version>\bin`
  von selbst, sonst `-IfwBinDir <Ordner>` angeben oder die Umgebungsvariable `QTIFWDIR` setzen.
- PowerShell (ist bei Windows 10/11 dabei)

## Bauen

```bat
install\make-installer.bat D:\Projekte\Qt5\mbChess\build\Desktop_Qt_6_12_0_MinGW_64_bit_Release
```

Ohne Build-Ordner sucht das Skript den neuesten Ordner mit `bin\mbchess.exe` neben dem
Quelltext. Weitere Schalter: `-QtBinDir`, `-IfwBinDir`, `-Languages de` (nur einen der Installer).

Oder als letzter Build-Schritt in Qt Creator: unter *Projekte → Build → qmake* bei
„Zusätzliche Argumente“ `CONFIG+=installer` eintragen.

## Was das Skript tut

1. `mbchess.exe` nach `install/build/app` kopieren und `windeployqt --compiler-runtime` laufen
   lassen (Qt-DLLs, Plugins, Qt-Übersetzungen, MinGW-Laufzeit). Dazu LICENSE, README und CREDITS
   sowie die leeren Ordner `Openings`, `Chessbooks` und `Syzygy` (Endspiel-Datenbanken, lädt
   mbChess selbst herunter) mit einer Lies-mich-Datei.
2. Die fertig gebauten Engines aus `bin\engines\<Name>` übernehmen, jede als eigene Komponente
   mit ihrer Lizenzdatei (Crafty nie, seine Lizenz verbietet die Weitergabe).
3. Die Sprachaufnahme `bin\mbchess-voicerecorder.exe` (wird beim Bauen von mbChess mitgebaut)
   mit eigenem `windeployqt`-Lauf übernehmen; in ihre Komponente kommen nur die Dateien, die
   mbChess nicht schon mitbringt. Fehlt sie, hat der Installer keine Sprachaufnahme-Komponente.
4. Das Handbuch aus `docs/manual` übernehmen (`mbChess-Handbuch.pdf` bzw. `mbChess-Manual.pdf`).
   Fehlt es, hat der Installer keine Handbuch-Komponente.
5. Pro Sprache `config.xml`, die `package.xml`-Dateien und das Steuerskript erzeugen.
6. `binarycreator --offline-only` aufrufen.

`install/build` wird bei jedem Lauf neu angelegt; `install/output` und `install/build` sind
nicht versioniert.

## Der Installer

- Willkommensseite: „Willkommen bei mb-Chess, dem vielseitigen Schachprogramm für Windows, Linux,
  MacOS und Android“, © Michael Bergmann 2026 und Lizenzhinweis; danach die Lizenzseite (GPL 3,
  im deutschen Installer mit deutscher Einleitung, dazu die Lizenzen der Engines).
- Zielordner: `C:\mbChess` (änderbar).
- Komponenten:
  - mbChess (immer)
  - UCI-Schach-Engines (Vorgabe: ja), darunter jede Engine einzeln abwählbar
  - Sprachaufnahme (Vorgabe: nein)
  - Handbuch (PDF) (Vorgabe: nein)
- Verknüpfungen im Startmenü (mbChess, Sprachaufnahme, Handbuch, Deinstallieren) und auf dem Desktop.
- Deinstallation: `mbChess-Deinstallation.exe` bzw. `mbChess-Uninstall.exe` im Programmordner,
  auch über *Einstellungen → Apps*.
- Ist mbChess im Zielordner schon installiert, entfernt der Installer die alte Installation
  vor dem Kopieren still (`purge`) und installiert neu. Der Ordner und eigene Dateien in
  `Openings`, `Chessbooks` und `Syzygy` bleiben erhalten (`RemoveTargetDir` ist aus).

Die Knöpfe des Assistenten („Weiter“, „Abbrechen“) übersetzt das Qt Installer Framework selbst
nach der Sprache von Windows. Alle Texte von mbChess stehen in der Sprache des jeweiligen Installers.

## Dateien

```
make-installer.bat / .ps1     Aufruf und eigentliches Skript
install.pro                   Build-Schritt für qmake CONFIG+=installer
config/controlscript.qs       Willkommenstext, Ersetzen einer alten Installation
packages/*/meta/installscript.qs   Verknüpfungen
license/license_de.txt, _en.txt    Einleitung vor dem GPL-Text
images/                       watermark.png, photo.png (Foto als PNG, QtIFW kann kein JPEG; rechts neben dem Willkommenstext, aus dem Installer-Ressourcenordner oder inline aus controlscript.qs), logo.png; make-images.sh erzeugt sie neu
```
