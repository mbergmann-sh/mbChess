# mbChess Beta-Downloadseite

Eine einzelne Seite, auf der Beta-Tester die aktuellen Builds für Windows und
Linux herunterladen. Jeder Download wird gezählt, die Zahlen stehen auf der Seite.
Die Seite hängt nicht vom Build ab: Du lädst sie einmal hoch und legst danach
neue Builds von Hand in den passenden Ordner, egal von welchem Rechner aus.

## Voraussetzungen

- Webspace mit **PHP 8.0 oder neuer** (bei üblichem Shared Hosting Standard)
- Apache (die `.htaccess`-Dateien sperren Build- und Datenordner). Bei nginx
  siehe unten.
- Keine Datenbank nötig, der Zähler ist eine JSON-Datei.

## Einmalig einrichten

1. Den **Inhalt** von `website/site/` per FTP in einen Ordner unter der
   Hauptdomain laden, z. B. nach `https://example.de/mbchess-beta/`. Der
   Ordnername ist frei wählbar, alle Pfade sind relativ.
2. Auf dem Server die Ordner `builds/windows/` und `builds/linux/` anlegen,
   falls das FTP-Programm leere Ordner nicht mit hochlädt.
3. Dem Ordner `data/` Schreibrechte für den Webserver geben (meist 755 oder
   775, bei manchen Hostern 777). Dort entstehen `counts.json` (Zähler) und
   `checksums.json` (Prüfsummen-Cache) von selbst.
4. Optional `config.php` anpassen: Titel, Kontaktadresse für Fehlerberichte,
   Anzahl der angezeigten früheren Builds.

Wenn du später die Seite selbst aktualisierst, lade `data/` nicht mit hoch,
sonst überschreibst du die Zähler. Das Repository enthält dort nur die
`.htaccess`, also passiert das nur, wenn du die JSON-Dateien vom Server
zurückkopiert hast.

## Neuen Build veröffentlichen

| System  | Datei aus dem Build                                   | Ordner auf dem Server |
|---------|-------------------------------------------------------|-----------------------|
| Windows | `install/output/mbChess-<Version>-Setup-de.exe` und `-en.exe` | `builds/windows/` |
| Linux   | `install/output/mbChess-<Version>-linux-x86_64.tar.gz` | `builds/linux/` |

Die Windows-Installer entstehen auf Windows mit `qmake CONFIG+=installer`
(oder `install\make-installer.bat`), das Linux-Archiv auf Linux mit
`qmake CONFIG+=installer` (oder
`python3 tools/make-linux-package.py --build-dir <Build-Ordner>`).
Jeder Rechner lädt nur seine eigenen Dateien hoch, die anderen bleiben
unberührt.

- Alle Dateien der **neuesten Version** werden als Hauptdownload gezeigt
  (z. B. Installer Deutsch und English). Die Versionsnummer liest die Seite
  aus dem Dateinamen (`0.1.30`), das Datum ist das Hochladedatum.
- Ältere Versionen erscheinen unter „Frühere Builds“. Löschen = Datei
  entfernen. Ihre Zählerstände bleiben erhalten und zählen zur Gesamtsumme.
- **Feste Namen** gehen auch, z. B. immer `mbChess-Setup-de.exe`,
  `mbChess-Setup-en.exe` und `mbChess-linux.tar.gz`: einfach überschreiben.
  Dann zeigt die Seite keine Versionsnummer, und der Zähler einer Datei zählt
  über alle Versionen weiter.
- Erlaubte Endungen: zip, exe, msi, 7z, AppImage, deb, tar.gz, tar.xz, tgz
  (in `config.php` änderbar). Andere Dateien werden ignoriert.

## Zähler

- Gezählt wird jeder Aufruf von `download.php` (nicht die Seite selbst).
- Direkte Links auf `builds/...` sind gesperrt, damit niemand am Zähler
  vorbei lädt.
- Es werden keine IP-Adressen oder sonstigen personenbezogenen Daten
  gespeichert.
- Zähler zurücksetzen: `data/counts.json` auf dem Server löschen oder durch
  `{}` ersetzen.

## Suchmaschinen

Die Seite trägt `noindex, nofollow`, damit sie nicht bei Google auftaucht.
Einen Passwortschutz hat sie nicht. Wer den Link kennt, kann laden. Wer das
will, kann im Hoster-Panel einen Verzeichnisschutz (`.htpasswd`) auf den
Ordner legen.

## nginx statt Apache

nginx liest keine `.htaccess`. Dann in der Server-Konfiguration ergänzen:

    location ~ ^/mbchess-beta/(builds|data)/ { deny all; }
    location ~ ^/mbchess-beta/(config|lib)\.php$ { deny all; }
