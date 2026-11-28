<?php
// mbChess Beta-Downloadseite: Einstellungen
// Neue Builds einfach per FTP in builds/windows/ bzw. builds/linux/ hochladen.
// Angezeigt wird jeweils die neueste Datei (nach Änderungsdatum), ältere
// Builds erscheinen darunter als "Frühere Builds".

date_default_timezone_set('Europe/Berlin');

return [
    'title'        => 'mbChess Beta',
    'subtitle'     => 'Testversionen für Beta-Tester',
    // Kontaktadresse für Fehlerberichte (leer lassen = nicht anzeigen)
    'contact'      => '',
    // Link zum Quellcode / Issue-Tracker (leer lassen = nicht anzeigen)
    'repo'         => 'https://github.com/mbergmann-sh/mbChess',
    // Erlaubte Dateiendungen für Builds
    'extensions'   => ['zip', 'exe', 'msi', '7z', 'appimage', 'deb', 'tar.gz', 'tar.xz', 'tgz'],
    // Wie viele Dateien früherer Builds pro System angezeigt werden (0 = keine)
    'older_builds' => 10,
    // SHA-256-Prüfsummen anzeigen (werden einmal berechnet und zwischengespeichert)
    'checksums'    => true,
];
