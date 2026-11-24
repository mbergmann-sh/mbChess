// SPDX-License-Identifier: GPL-3.0-or-later
#include "languagemanager.h"

#include <compat/compat.h>

#include <QCoreApplication>
#include <QLocale>
#include <QSettings>

LanguageManager::LanguageManager(QObject *parent)
    : QObject(parent)
{
}

QString LanguageManager::savedLanguage()
{
    return QSettings().value(QStringLiteral("ui/language")).toString();
}

void LanguageManager::setLanguage(const QString &code)
{
    QString lang = code;
    if (lang.isEmpty())
        lang = QLocale::system().language() == QLocale::German ? QStringLiteral("de") : QStringLiteral("en");
    if (lang != QLatin1String("de") && lang != QLatin1String("en"))
        lang = QStringLiteral("en");
    if (lang == m_language)
        return;

    QCoreApplication::removeTranslator(&m_appTranslator);
    QCoreApplication::removeTranslator(&m_qtTranslator);

    // Program texts are embedded as resources (CONFIG += embed_translations).
    if (m_appTranslator.load(QStringLiteral(":/i18n/mbchess_%1.qm").arg(lang)))
        QCoreApplication::installTranslator(&m_appTranslator);
    // Qt's own texts (Yes/No/Cancel of the standard dialogs): embedded by app.pro, else
    // next to the program (windeployqt: translations/qt_de.qm) or in the Qt installation.
    if (lang != QLatin1String("en")) {
        const QString appDir = QCoreApplication::applicationDirPath() + QStringLiteral("/translations");
        const QString qtDir = mbc::compat::qtTranslationsPath();
        const bool loaded = m_qtTranslator.load(QStringLiteral(":/i18n/qtbase_%1.qm").arg(lang))
            || m_qtTranslator.load(QStringLiteral("qtbase_%1").arg(lang), appDir)
            || m_qtTranslator.load(QStringLiteral("qt_%1").arg(lang), appDir)
            || m_qtTranslator.load(QStringLiteral("qtbase_%1").arg(lang), qtDir)
            || m_qtTranslator.load(QStringLiteral("qt_%1").arg(lang), qtDir);
        if (loaded)
            QCoreApplication::installTranslator(&m_qtTranslator);
    }

    m_language = lang;
    if (!code.isEmpty())
        QSettings().setValue(QStringLiteral("ui/language"), lang);
}

QString LanguageManager::pieceLetters() const
{
    //: Piece letters for King, Queen, Rook, Bishop, Knight in move notation (exactly 5 letters)
    return tr("KQRBN");
}
