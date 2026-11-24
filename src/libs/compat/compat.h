// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - small helpers that hide differences between Qt 5.15 and Qt 6.
// Rule: code outside this file must not use #if QT_VERSION; add a helper here instead.
#pragma once

#include <QtGlobal>
#include <QCoreApplication>
#include <QLibraryInfo>
#include <QDropEvent>
#include <QMouseEvent>
#include <QPoint>
#include <QString>

namespace mbc::compat {

// Directory with Qt's own translations (qtbase_de.qm etc.).
inline QString qtTranslationsPath()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return QLibraryInfo::path(QLibraryInfo::TranslationsPath);
#else
    return QLibraryInfo::location(QLibraryInfo::TranslationsPath);
#endif
}

// Qt 6 always scales for high-DPI screens; Qt 5 needs these attributes before
// the QApplication object is created.
inline void enableHighDpi()
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QCoreApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
#endif
}

// Mouse position in widget coordinates (Qt 6: position(), Qt 5: pos()).
inline QPoint mousePos(const QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

// Mouse position in screen coordinates.
inline QPoint mouseGlobalPos(const QMouseEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->globalPosition().toPoint();
#else
    return e->globalPos();
#endif
}

// Position of a drag & drop event in widget coordinates.
inline QPoint dropPos(const QDropEvent *e)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return e->position().toPoint();
#else
    return e->pos();
#endif
}

} // namespace mbc::compat
