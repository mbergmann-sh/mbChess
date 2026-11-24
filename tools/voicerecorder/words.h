// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess voice recorder - the words mbChess announces (see src/app/soundplayer.h).
// The file name is fixed; the text is what the speaker reads out.
#pragma once

#include <QString>
#include <QVector>

struct Word
{
    const char *file;   // <file>.wav
    const char *de;     // spoken text, German
    const char *en;     // spoken text, English
    const char *hintDe; // explanation for the speaker
    const char *hintEn;
};

inline const QVector<Word> &announcementWords()
{
    static const QVector<Word> words = {
        { "welcome", "Willkommen bei mbChess!", "Welcome to mbChess!",
          "Begrüßung beim Programmstart – gern auch ein eigener Satz", "Greeting when the program starts – your own sentence is fine too" },
        { "a", "a", "a", "Linie a (wie in „a4“)", "file a (as in \"a4\")" },
        { "b", "b", "b", "Linie b", "file b" },
        { "c", "c", "c", "Linie c", "file c" },
        { "d", "d", "d", "Linie d", "file d" },
        { "e", "e", "e", "Linie e", "file e" },
        { "f", "f", "f", "Linie f", "file f" },
        { "g", "g", "g", "Linie g", "file g" },
        { "h", "h", "h", "Linie h", "file h" },
        { "1", "eins", "one", "Reihe 1", "rank 1" },
        { "2", "zwei", "two", "Reihe 2 (deutlicher: „zwo“)", "rank 2" },
        { "3", "drei", "three", "Reihe 3", "rank 3" },
        { "4", "vier", "four", "Reihe 4", "rank 4" },
        { "5", "fünf", "five", "Reihe 5", "rank 5" },
        { "6", "sechs", "six", "Reihe 6", "rank 6" },
        { "7", "sieben", "seven", "Reihe 7", "rank 7" },
        { "8", "acht", "eight", "Reihe 8", "rank 8" },
        { "king", "König", "king", "Figur", "piece" },
        { "queen", "Dame", "queen", "Figur, auch bei der Umwandlung", "piece, also for promotion" },
        { "rook", "Turm", "rook", "Figur", "piece" },
        { "bishop", "Läufer", "bishop", "Figur", "piece" },
        { "knight", "Springer", "knight", "Figur", "piece" },
        { "pawn", "Bauer", "pawn", "Figur", "piece" },
        { "takes", "schlägt", "takes", "Schlagzug, z. B. „Springer schlägt f3“", "capture, e.g. \"knight takes f3\"" },
        { "check", "Schach", "check", "nach einem Zug, der Schach bietet", "after a move that gives check" },
        { "mate", "matt", "checkmate", "nach dem Mattzug", "after the mating move" },
        { "o-o", "kurze Rochade", "castles kingside", "", "" },
        { "o-o-o", "lange Rochade", "castles queenside", "", "" },
        { "castling", "Rochade", "castling", "", "" },
        { "stalemate", "Patt", "stalemate", "Partieende", "end of the game" },
        { "draw", "Remis", "draw", "Partieende", "end of the game" },
    };
    return words;
}
