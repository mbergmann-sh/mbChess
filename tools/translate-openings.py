#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""German names for the openings of lichess chess-openings (src/app/resources/openings).

    python3 tools/translate-openings.py src/app/resources/openings

Writes names_de.tsv (English name <TAB> German name) into that folder. The usual German
names of the opening families come from a table; variations are translated word by word
(e.g. "Najdorf Variation" -> "Najdorf-Variante", "Exchange Variation" -> "Abtauschvariante").
Fix wrong names in the tables below and run the script again.
"""
import glob
import os
import re
import sys

FAMILY = {
    "Sicilian Defense": "Sizilianische Verteidigung",
    "Ruy Lopez": "Spanische Partie",
    "French Defense": "Französische Verteidigung",
    "Queen's Gambit Declined": "Abgelehntes Damengambit",
    "Queen's Gambit Accepted": "Angenommenes Damengambit",
    "Queen's Gambit": "Damengambit",
    "Italian Game": "Italienische Partie",
    "English Opening": "Englische Eröffnung",
    "King's Gambit Accepted": "Angenommenes Königsgambit",
    "King's Gambit Declined": "Abgelehntes Königsgambit",
    "King's Gambit": "Königsgambit",
    "King's Indian Defense": "Königsindische Verteidigung",
    "King's Indian Attack": "Königsindischer Angriff",
    "Caro-Kann Defense": "Caro-Kann-Verteidigung",
    "Nimzo-Indian Defense": "Nimzoindische Verteidigung",
    "Queen's Pawn Game": "Damenbauernspiel",
    "Queen's Pawn": "Damenbauernspiel",
    "Semi-Slav Defense": "Halbslawische Verteidigung",
    "Semi-Slav Defense Accepted": "Angenommene Halbslawische Verteidigung",
    "Dutch Defense": "Holländische Verteidigung",
    "Benoni Defense": "Benoni-Verteidigung",
    "Grünfeld Defense": "Grünfeld-Indische Verteidigung",
    "Neo-Grünfeld Defense": "Neo-Grünfeld-Verteidigung",
    "Queen's Indian Defense": "Damenindische Verteidigung",
    "Queen's Indian Accelerated": "Beschleunigte Damenindische Verteidigung",
    "Pseudo Queen's Indian Defense": "Pseudo-Damenindische Verteidigung",
    "Alekhine Defense": "Aljechin-Verteidigung",
    "Scotch Game": "Schottische Partie",
    "Indian Defense": "Indische Verteidigung",
    "Slav Defense": "Slawische Verteidigung",
    "Slav Indian": "Slawisch-Indisch",
    "Petrov's Defense": "Russische Verteidigung",
    "Four Knights Game": "Vierspringerspiel",
    "Three Knights Opening": "Dreispringerspiel",
    "Zukertort Opening": "Zukertort-Eröffnung",
    "Scandinavian Defense": "Skandinavische Verteidigung",
    "Philidor Defense": "Philidor-Verteidigung",
    "Réti Opening": "Réti-Eröffnung",
    "Nimzowitsch Defense": "Nimzowitsch-Verteidigung",
    "Vienna Game": "Wiener Partie",
    "Vienna Gambit": "Wiener Gambit",
    "Bishop's Opening": "Läuferspiel",
    "Modern Defense": "Moderne Verteidigung",
    "King's Pawn Game": "Königsbauernspiel",
    "King's Pawn Opening": "Königsbauernspiel",
    "King's Knight Opening": "Königsspringerspiel",
    "Pterodactyl Defense": "Pterodactylus-Verteidigung",
    "Catalan Opening": "Katalanische Eröffnung",
    "Pirc Defense": "Pirc-Verteidigung",
    "Bird Opening": "Bird-Eröffnung",
    "Tarrasch Defense": "Tarrasch-Verteidigung",
    "Polish Opening": "Polnische Eröffnung (Sokolski)",
    "Polish Defense": "Polnische Verteidigung",
    "Bogo-Indian Defense": "Bogoljubow-Indische Verteidigung",
    "Old Indian Defense": "Altindische Verteidigung",
    "East Indian Defense": "Ostindische Verteidigung",
    "Hungarian Opening": "Ungarische Eröffnung",
    "Nimzo-Larsen Attack": "Nimzowitsch-Larsen-Angriff",
    "Center Game": "Mittelgambit",
    "Center Game Accepted": "Angenommenes Mittelgambit",
    "Latvian Gambit": "Lettisches Gambit",
    "Latvian Gambit Accepted": "Angenommenes Lettisches Gambit",
    "Danish Gambit": "Nordisches Gambit",
    "Danish Gambit Accepted": "Angenommenes Nordisches Gambit",
    "Danish Gambit Declined": "Abgelehntes Nordisches Gambit",
    "Benko Gambit": "Wolga-Gambit (Benkö)",
    "Benko Gambit Accepted": "Angenommenes Wolga-Gambit (Benkö)",
    "Benko Gambit Declined": "Abgelehntes Wolga-Gambit (Benkö)",
    "Blumenfeld Countergambit": "Blumenfeld-Gambit",
    "Blumenfeld Countergambit Accepted": "Angenommenes Blumenfeld-Gambit",
    "Richter-Veresov Attack": "Richter-Weressow-Angriff",
    "Elephant Gambit": "Elefantengambit",
    "Englund Gambit": "Englund-Gambit",
    "Englund Gambit Declined": "Abgelehntes Englund-Gambit",
    "Rat Defense": "Rattenverteidigung",
    "English Defense": "Englische Verteidigung",
    "English Orangutan": "Orang-Utan (Sokolski)",
    "Hippopotamus Defense": "Hippopotamus-Verteidigung",
    "Kangaroo Defense": "Känguru-Verteidigung",
    "Lion Defense": "Löwen-Verteidigung",
    "Vulture Defense": "Geier-Verteidigung",
    "Lemming Defense": "Lemming-Verteidigung",
    "Fried Fox Defense": "Gebratener-Fuchs-Verteidigung",
    "Creepy Crawly Formation": "Kriechtier-Aufstellung",
    "Bongcloud Attack": "Bongcloud-Angriff",
    "Amazon Attack": "Amazonen-Angriff",
    "Sodium Attack": "Natrium-Angriff",
    "Global Opening": "Globale Eröffnung",
    "Lasker Simul Special": "Lasker-Simultan-Spezial",
    "Formation": "Aufstellung",
    "Barnes Opening": "Barnes-Eröffnung",
    "Mexican Defense": "Mexikanische Verteidigung",
    "Australian Defense": "Australische Verteidigung",
    "Czech Defense": "Tschechische Verteidigung",
    "Zaire Defense": "Zaire-Verteidigung",
    "Irish Gambit": "Irisches Gambit",
    "Portuguese Opening": "Portugiesische Eröffnung",
}

PHRASE = {
    "Main Line": "Hauptvariante",
    "Exchange Variation": "Abtauschvariante",
    "Advance Variation": "Vorstoßvariante",
    "Two Knights Defense": "Zweispringerspiel im Nachzuge",
    "Two Knights Attack": "Zweispringerangriff",
    "Dragon Variation": "Drachenvariante",
    "Accelerated Dragon": "Beschleunigter Drache",
    "Hyperaccelerated Dragon": "Hyperbeschleunigter Drache",
    "Open": "Offene Variante",
    "Closed": "Geschlossene Variante",
    "Poisoned Pawn Variation": "Variante mit vergiftetem Bauern",
    "Poisoned Pawn": "Vergifteter Bauer",
    "Fried Liver Attack": "Fegatello-Angriff",
    "Hedgehog Defense": "Igel",
    "Hedgehog Variation": "Igel-Variante",
    "Hedgehog System": "Igel-System",
    "Wing Gambit": "Flügelgambit",
    "Panov Attack": "Panow-Angriff",
    "Panov-Botvinnik Attack": "Panow-Botwinnik-Angriff",
    "Giuoco Piano": "Giuoco Piano",
    "Giuoco Pianissimo": "Giuoco Pianissimo",
    "Normal Variation": "Normalvariante",
    "Modern Line": "Moderne Linie",
    "Retreat Variation": "Rückzugsvariante",
    "Four Pawns Attack": "Vierbauernangriff",
    "Four Knights Variation": "Vierspringervariante",
    "Three Knights Variation": "Dreispringervariante",
    "Two Knights Variation": "Zweispringervariante",
    "Bishop's Gambit": "Läufergambit",
    "Knight's Gambit": "Springergambit",
    "Scotch Gambit": "Schottisches Gambit",
    "Smith-Morra Gambit": "Smith-Morra-Gambit",
    "Smith-Morra Gambit Accepted": "Angenommenes Smith-Morra-Gambit",
    "Smith-Morra Gambit Declined": "Abgelehntes Smith-Morra-Gambit",
    "Meran Variation": "Meraner Variante",
    "Scheveningen Variation": "Scheveninger Variante",
    "St. Petersburg Variation": "Petersburger Variante",
    "Falkbeer Countergambit": "Falkbeer-Gegengambit",
    "Albin Countergambit": "Albins Gegengambit",
    "Muzio Gambit": "Muzio-Gambit",
    "Stonewall Attack": "Stonewall-Angriff",
    "Stonewall Variation": "Stonewall-Variante",
    "Stonewall": "Stonewall",
    "Fianchetto Variation": "Fianchetto-Variante",
    "Symmetrical Variation": "Symmetrische Variante",
    "King's English Variation": "Königsenglisch",
    "King's Indian Attack": "Königsindischer Angriff",
    "Queen's Gambit": "Damengambit",
    "Spanish Variation": "Spanische Variante",
    "Rubinstein Variation": "Rubinstein-Variante",
    "Winawer Variation": "Winawer-Variante",
    "Anglo-Slav Variation": "Anglo-Slawische Variante",
    "Anglo-Indian Defense": "Anglo-Indische Verteidigung",
    "Orthodox Defense": "Orthodoxe Verteidigung",
    "Mainline": "Hauptvariante",
    "Deferred": "verzögert",
    "Accepted": "angenommen",
    "Declined": "abgelehnt",
    "Refused": "abgelehnt",
    "Countergambit": "Gegengambit",
    "Semi-Slav": "Halbslawisch",
    "Reversed Rat": "Umgekehrte Ratte",
    "Modern Variations": "Moderne Varianten",
    "Rare Defenses": "Seltene Verteidigungen",
    "Anti-Qxd4 Move Order": "Anti-Dxd4-Zugfolge",
    "Anti-Qxd4 Move Order Accepted": "Anti-Dxd4-Zugfolge (angenommen)",
}
AFTER = {"Reversed": "(umgekehrt)", "Accepted": "(angenommen)", "Declined": "(abgelehnt)"}

# noun -> (German, gender)
NOUN = {
    "Variation": ("Variante", "f"), "Defense": ("Verteidigung", "f"), "Attack": ("Angriff", "m"),
    "Gambit": ("Gambit", "n"), "Countergambit": ("Gegengambit", "n"), "Opening": ("Eröffnung", "f"),
    "Game": ("Partie", "f"), "System": ("System", "n"), "Line": ("Linie", "f"), "Trap": ("Falle", "f"),
    "Formation": ("Aufstellung", "f"), "Variant": ("Variante", "f"), "Setup": ("Aufbau", "m"),
    "Structure": ("Struktur", "f"), "Complex": ("Komplex", "m"), "Approach": ("Aufbau", "m"),
}
ENDING = {"f": "e", "m": "er", "n": "es"}
# adjectives (stem gets the ending) and place adjectives without ending
ADJ = {
    "Classical": "Klassisch", "Modern": "Modern", "Old": "Alt", "Accelerated": "Beschleunigt",
    "Symmetrical": "Symmetrisch", "Normal": "Normal", "Reversed": "Umgekehrt", "Improved": "Verbessert",
    "Positional": "Positionell", "Orthodox": "Orthodox", "Spanish": "Spanisch", "French": "Französisch",
    "English": "Englisch", "Italian": "Italienisch", "Scotch": "Schottisch", "Russian": "Russisch",
    "Dutch": "Holländisch", "Slav": "Slawisch", "Indian": "Indisch", "Hungarian": "Ungarisch",
    "Polish": "Polnisch", "Danish": "Dänisch", "Swedish": "Schwedisch", "Yugoslav": "Jugoslawisch",
    "Czech": "Tschechisch", "Mexican": "Mexikanisch", "Australian": "Australisch",
    "Portuguese": "Portugiesisch", "Irish": "Irisch", "Basque": "Baskisch", "Little": "Klein",
    "Long": "Lang", "Short": "Kurz", "Quiet": "Ruhig", "Wild": "Wild", "Fast": "Schnell",
    "Delayed": "Verzögert", "Deferred": "Verzögert", "Early": "Früh", "Rare": "Selten",
    "Sharp": "Scharf", "Hyperaccelerated": "Hyperbeschleunigt", "Big": "Groß", "Grand": "Groß",
    "Main": "Haupt", "Closed": "Geschlossen", "Open": "Offen", "Half-Open": "Halboffen",
    "Semi-Open": "Halboffen", "Central": "Zentral", "Reverse": "Umgekehrt", "Double": "Doppelt",
    "Modified": "Verändert", "Exotic": "Exotisch", "Anti-": "Anti", "Romanian": "Rumänisch",
    "German": "Deutsch", "Austrian": "Österreichisch", "Belgian": "Belgisch", "Swiss": "Schweizerisch",
    "Greek": "Griechisch", "Bulgarian": "Bulgarisch", "Hungarian-": "Ungarisch", "Catalan": "Katalanisch",
    "Icelandic": "Isländisch", "Norwegian": "Norwegisch", "Finnish": "Finnisch", "Brazilian": "Brasilianisch",
    "American": "Amerikanisch", "Canadian": "Kanadisch", "Chinese": "Chinesisch", "Japanese": "Japanisch",
    "Argentine": "Argentinisch", "Lithuanian": "Litauisch", "Estonian": "Estnisch", "Croatian": "Kroatisch",
    "Slovak": "Slowakisch", "Polish-": "Polnisch", "Dragon": "Drachen", "Accepted": "Angenommen",
    "Declined": "Abgelehnt", "Refused": "Abgelehnt", "Neo-Classical": "Neoklassisch",
    "Baltic": "Baltisch", "Semi-Slav": "Halbslawisch", "Anglo-Slav": "Anglo-Slawisch", "Anglo-Indian": "Anglo-Indisch",
}
PLACE = {
    "Berlin": "Berliner", "Vienna": "Wiener", "Dresden": "Dresdner", "Leningrad": "Leningrader",
    "Moscow": "Moskauer", "Paris": "Pariser", "Budapest": "Budapester", "Amsterdam": "Amsterdamer",
    "Prague": "Prager", "Meran": "Meraner", "Scheveningen": "Scheveninger", "Hamburg": "Hamburger",
    "Cambridge": "Cambridge-", "Manhattan": "Manhattan-", "Stockholm": "Stockholmer", "Zurich": "Züricher",
    "Rotterdam": "Rotterdamer", "Kiev": "Kiewer", "Marienbad": "Marienbader",
}
# first parts of compound nouns
COMPOUND = {
    "Exchange": "Abtausch", "Advance": "Vorstoß", "Fianchetto": "Fianchetto-", "Center": "Zentrums",
    "Counter": "Gegen", "Pawn": "Bauern", "Knight": "Springer", "Bishop": "Läufer", "Rook": "Turm",
    "Queen": "Damen", "King": "Königs", "Two Knights": "Zweispringer", "Three Knights": "Dreispringer",
    "Four Knights": "Vierspringer", "Four Pawns": "Vierbauern", "Three Pawns": "Dreibauern",
    "Two Pawns": "Zweibauern", "Wing": "Flügel", "Pawn Storm": "Bauernsturm", "Queenside": "Damenflügel",
    "Kingside": "Königsflügel", "Retreat": "Rückzugs", "Pin": "Fessel", "Hedgehog": "Igel",
    "King's": "Königs", "Queen's": "Damen", "Bishop's": "Läufer", "Knight's": "Springer", "Rook's": "Turm",
    "Attack": "Angriffs", "Defense": "Verteidigungs", "Main": "Haupt", "Counterattack": "Gegenangriff", "Fork": "Gabel", "Mate": "Matt", "Endgame": "Endspiel",
}
# German spelling of names (mostly Russian names, transliterated the German way)
NAME = {
    "Botvinnik": "Botwinnik", "Smyslov": "Smyslow", "Kholmov": "Cholmow", "Alekhine": "Aljechin",
    "Tolush": "Tolusch", "Levenfish": "Löwenfisch", "Kasparov": "Kasparow", "Petrosian": "Petrosjan",
    "Spassky": "Spasski", "Taimanov": "Taimanow", "Sveshnikov": "Sweschnikow", "Chigorin": "Tschigorin",
    "Petrov": "Petrow", "Karpov": "Karpow", "Polugaevsky": "Polugajewski", "Romanishin": "Romanischin",
    "Kotov": "Kotow", "Boleslavsky": "Boleslawski", "Ragozin": "Ragosin", "Zaitsev": "Saizew",
    "Belavenets": "Belawenez", "Averbakh": "Awerbach", "Makogonov": "Makogonow", "Veresov": "Weressow",
    "Chameleon": "Chamäleon", "Endgame": "Endspiel", "Panov": "Panow", "Geller": "Geller",
    "Kramnik": "Kramnik", "Lilienthal": "Lilienthal", "Simagin": "Simagin", "Suetin": "Suetin",
    "Tartakower": "Tartakower", "Bronstein": "Bronstein", "Korchnoi": "Kortschnoi", "Tseshkovsky": "Zeschkowski",
    "Yusupov": "Jussupow", "Vitolins": "Vitolinš", "Kozul": "Kožul", "Shirov": "Schirow", "Anand": "Anand",
    "Svidler": "Swidler", "Ivanchuk": "Iwantschuk", "Beliavsky": "Beljawski", "Tal": "Tal",
    "Grischuk": "Grischtschuk", "Nezhmetdinov": "Neschmetdinow", "Keres": "Keres", "Flohr": "Flohr",
    "Kan": "Kan", "Paulsen": "Paulsen", "Sozin": "Sosin", "Rauzer": "Rauser", "Gurgenidze": "Gurgenidse",
    "Ufimtsev": "Ufimzew", "Bogoljubov": "Bogoljubow", "Smirnov": "Smirnow", "Lasker": "Lasker",
    "Steinitz": "Steinitz", "Mikenas": "Mikėnas", "Ponomariov": "Ponomarjow", "Morozevich": "Morosewitsch",
    "Sokolsky": "Sokolski", "Dzindzi": "Dzindzichashvili", "Lputian": "Lputjan", "Kmoch": "Kmoch",
}
PIECES = str.maketrans({"Q": "D", "R": "T", "B": "L", "N": "S"})


def move_text(token):
    if re.fullmatch(r"[KQRBN]?[a-h]?[1-8]?x?[a-h][1-8](=[QRBN])?[+#]?|O-O(-O)?", token):
        return token.translate(PIECES)
    return token


def adjective(word, gender):
    if word in PLACE:
        return PLACE[word]
    return ADJ[word] + ENDING[gender]


def phrase(text):
    """One part such as 'Najdorf Variation' or 'Classical Defense, Main Line'."""
    if text in PHRASE:
        return PHRASE[text]
    if text in FAMILY:
        return FAMILY[text]
    words = text.split()
    if words and words[0] == "with":
        rest = words[1:]
        if all(move_text(w) != w or re.fullmatch(r"[a-h][1-8]|[KQRBN]?[a-h]?x?[a-h][1-8][+#]?", w) for w in rest):
            return "mit " + " ".join(move_text(w) for w in rest)
        return "mit " + phrase(" ".join(rest))
    if words and words[-1] == "Line" and len(words) >= 2 and all(re.fullmatch(r"[KQRBNa-hx1-8+#=O-]+", w) for w in words[:-1]):
        return "Linie mit " + " ".join(move_text(w) for w in words[:-1])
    # status at the end: "... Gambit Accepted"
    status = None
    if len(words) >= 2 and words[-1] in ("Accepted", "Declined", "Refused", "Deferred"):
        status = words.pop()
    # the noun: last known noun word
    idx = max((i for i, w in enumerate(words) if w in NOUN), default=-1)
    if idx < 0:
        out = " ".join(move_text(w) for w in words)
        return out + (" (" + PHRASE[status] + ")" if status else "")
    noun, gender = NOUN[words[idx]]
    before, after = words[:idx], words[idx + 1:]
    adjs, comp = [], []
    i = 0
    while i < len(before):
        two = " ".join(before[i:i + 2])
        if two in COMPOUND:
            comp.append(COMPOUND[two])
            i += 2
            continue
        w = before[i]
        if not comp and (w in ADJ or w in PLACE) and w not in COMPOUND:
            adjs.append(adjective(w, gender))
        elif w in COMPOUND:
            comp.append(COMPOUND[w])
        else:
            comp.append(w)
        i += 1
    if status in ("Accepted", "Declined", "Refused"):
        adjs.insert(0, adjective(status, gender))
    if comp:
        # names are joined with hyphens: Najdorf-Variante, Max-Lange-Angriff;
        # German compound parts directly: Abtauschvariante
        german = all(c in COMPOUND.values() for c in comp)
        if german:
            word = comp[0] + "".join(c.lower() for c in comp[1:]) + (noun.lower() if not comp[-1].endswith("-") else noun)
            word = word[0].upper() + word[1:]
        else:
            word = "-".join(NAME.get(c, c).rstrip("-") for c in comp) + "-" + noun
    else:
        word = noun
    out = " ".join(adjs + [word])
    if after:
        out += " " + " ".join(AFTER.get(w, move_text(w)) for w in after)
    if status == "Deferred":
        out += " (verzögert)"
    return out


def translate(name):
    family, _, rest = name.partition(": ")
    parts = []
    # the family may itself contain ", with ..."
    for i, part in enumerate(family.split(", ")):
        parts.append(phrase(part))
    head = ", ".join(parts)
    if not rest:
        return head
    return head + ": " + ", ".join(phrase(p) for p in rest.split(", "))


def main(folder):
    names = []
    for path in sorted(glob.glob(os.path.join(folder, "*.tsv"))):
        if os.path.basename(path).startswith("names_"):
            continue
        with open(path, encoding="utf-8") as f:
            for line in f:
                cols = line.rstrip("\n").split("\t")
                if len(cols) >= 3 and cols[0] != "eco" and cols[1] not in names:
                    names.append(cols[1])
    with open(os.path.join(folder, "names_de.tsv"), "w", encoding="utf-8") as f:
        for n in names:
            f.write(f"{n}\t{translate(n)}\n")
    print(len(names), "names")


if __name__ == "__main__":
    if len(sys.argv) != 2:
        sys.exit(__doc__)
    main(sys.argv[1])
