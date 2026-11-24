// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - switches the UI language (German / English) at runtime.
#pragma once

#include <QObject>
#include <QTranslator>

class LanguageManager : public QObject
{
    Q_OBJECT
public:
    explicit LanguageManager(QObject *parent = nullptr);

    // "de" or "en". An empty string picks the system language (German if it is German, else English).
    void setLanguage(const QString &code);
    QString language() const { return m_language; }

    // Piece letters K, Q, R, B, N in the current language ("KDTLS" in German).
    QString pieceLetters() const;

    static QString savedLanguage();

private:
    QTranslator m_appTranslator;
    QTranslator m_qtTranslator;
    QString m_language;
};
