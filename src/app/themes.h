// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - board themes, piece sets and sound sets.
//
// Built-in themes are compiled into the program (resources/themes, converted from
// AmiChess with tools/amiga2themes.py). More can be put into the folder "themes" next
// to the program, with the same layout:
//     themes/boards/<Name>/light.png, dark.png   (one square, or a larger texture)
//     themes/<kind>/<Name>/theme.ini             (optional, UTF-8; see ThemeInfo)
//     themes/pieces/<Name>/wK.svg|png ... bP     (12 files)
//     themes/sounds/<Name>/<word>.wav            (a..h, 1..8, pawn, knight, ..., takes, check, mate, tap)
#pragma once

#include <QColor>
#include <QHash>
#include <QString>
#include <QStringList>

namespace themes {

// Contents of theme.ini, section [theme]:
//   name=Azul                    shown in the menu (default: the folder name)
//   group=Lucas Chess/Lucas      submenus, separated by '/'
//   light=#d9e0d8, dark=...      square colours (boards; textures are drawn on top)
//   frame=#..., coordinates=#... frame and coordinate colours (boards)
//   border=#a3a3a3               thin line around the squares (boards)
//   texture=square|auto          square: each texture fills one square; auto: small
//                                textures one square, large ones run across the board
struct ThemeInfo
{
    QString id;      // folder name, saved in the settings
    QString name;
    QString group;
    QString folder;
    QHash<QString, QString> values;

    QColor color(const QString &key) const;  // invalid if not set
};

ThemeInfo boardInfo(const QString &id);
ThemeInfo pieceInfo(const QString &id);

// The default board is a folder theme (flat grey). Built-in names that are not
// folders: the drawn wooden board, the SVG pieces and the same pieces on wooden discs.
inline QString defaultBoard() { return QStringLiteral("mbChess"); }
inline QString classicBoard() { return QStringLiteral("mbChess-classic"); }
inline QString defaultPieces() { return QStringLiteral("mbchess-discs"); }
inline QString classicPieces() { return QStringLiteral("cburnett"); }
inline QString defaultSounds() { return QStringLiteral("amichess"); }

// Folders searched for themes: the resources first, then <appdir>/themes.
QStringList searchFolders();

QStringList boardThemes();   // defaultBoard() first, then all found, sorted
QStringList pieceSets();     // defaultPieces() first
QStringList soundSets();

// Folder of a theme ("" if not found). The built-in piece sets live in the resources too.
QString boardFolder(const QString &name);
QString pieceFolder(const QString &name);
QString soundFolder(const QString &name);

} // namespace themes
