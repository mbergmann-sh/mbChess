# mbChess-Sprachaufnahme / voice recorder

Mit diesem kleinen Programm (Qt 6) sprichst du die Zugansagen von mbChess selbst ein.
Es fragt alle Wörter der Reihe nach ab: die Begrüßung beim Programmstart, die Linien a–h,
die Reihen 1–8, die Figuren, „schlägt“, „Schach“, „matt“, die Rochaden, „Patt“ und „Remis“.

## Bauen

Wird mit mbChess mitgebaut: `mbChess.pro` enthält den Build-Schritt `tools/voicerecorder-build.pro`,
der `mbchess-voicerecorder(.exe)` neben `mbchess(.exe)` in `bin/` legt, aber **nur, wenn das
Programm dort noch nicht existiert**. Neu bauen: die Datei löschen und erneut bauen, oder
`tools/voicerecorder/voicerecorder.pro` allein in Qt Creator öffnen. Braucht Qt 6 mit Qt
Multimedia (mit Qt 5 wird der Schritt mit einer Meldung übersprungen); weglassen mit
`qmake CONFIG+=no_voicerecorder`. Der Windows-Installer bietet das Werkzeug als optionale
Komponente „Sprachaufnahme“ an.

## Benutzen

1. Name der Stimme und Sprache wählen. Der Ordner ist `themes/sounds/<Name>` neben mbChess,
   wenn das Programm neben `mbchess.exe` liegt, sonst `Dokumente/mbChess-Stimmen/<Name>`
   (mit „Ändern...“ auf `C:\mbChess\themes\sounds` stellen).
2. Wort in der Liste wählen, **Leertaste** drücken, sprechen, **Leertaste** drücken.
   Die Aufnahme wird gespeichert und gleich abgespielt.
3. **Enter** oder „Abspielen“ spielt eine Aufnahme ab; „Neu aufnehmen“ ersetzt sie.
4. „Probe-Ansage“ spielt einen ganzen Zug (z. B. `Nxf3+` oder `Sxf3+`) so ab, wie mbChess ihn sagt.
5. In mbChess: *Brett → Stimme → <Name>*, dazu *Brett → Züge ansagen*. Ist `welcome.wav`
   aufgenommen, begrüßt mbChess dich beim Start.

Die Aufnahmen werden als WAV (48 kHz, mono, 16 Bit) gespeichert. „Stille abschneiden und
Lautstärke angleichen“ schneidet Pausen am Anfang und Ende ab, entfernt Gleichspannung und
hebt die Lautstärke auf -1 dB an. `theme.ini` im Ordner hält den Namen der Stimme.

Tipps: ruhiger Raum, Mikrofon etwa eine Handbreit vom Mund, deutlich und etwas langsamer
sprechen. Für „zwei“ ist „zwo“ besser zu verstehen.
