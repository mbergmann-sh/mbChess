// SPDX-License-Identifier: GPL-3.0-or-later
#include "guitheme.h"

#include <QApplication>
#include <QFile>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QStyle>
#include <QStyleFactory>
#include <QSvgRenderer>

namespace guitheme {

namespace {

QString g_current = defaultId();
QString g_nativeStyle;   // the style Qt chose at start-up

// The dark design, taken from AmigaED 4 ("Visual Studio Code Dark"): the colours of
// VS Code's "Dark+".
QPalette darkPalette()
{
    QPalette p;
    p.setColor(QPalette::Window, QColor(0x25, 0x25, 0x26));
    p.setColor(QPalette::WindowText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Base, QColor(0x1e, 0x1e, 0x1e));
    p.setColor(QPalette::AlternateBase, QColor(0x2d, 0x2d, 0x2d));
    p.setColor(QPalette::ToolTipBase, QColor(0x25, 0x25, 0x26));
    p.setColor(QPalette::ToolTipText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::Text, QColor(0xd4, 0xd4, 0xd4));
    p.setColor(QPalette::Button, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::ButtonText, QColor(0xcc, 0xcc, 0xcc));
    p.setColor(QPalette::BrightText, QColor(0xf4, 0x84, 0x71));
    p.setColor(QPalette::Link, QColor(0x37, 0x94, 0xff));
    p.setColor(QPalette::LinkVisited, QColor(0xb1, 0x80, 0xd7));
    p.setColor(QPalette::Highlight, QColor(0x09, 0x47, 0x71));
    p.setColor(QPalette::HighlightedText, QColor(0xff, 0xff, 0xff));
    p.setColor(QPalette::PlaceholderText, QColor(0x80, 0x80, 0x80));
    p.setColor(QPalette::Mid, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::Disabled, QPalette::WindowText, QColor(0x6a, 0x6a, 0x6a));
    p.setColor(QPalette::Disabled, QPalette::Text, QColor(0x6a, 0x6a, 0x6a));
    p.setColor(QPalette::Disabled, QPalette::ButtonText, QColor(0x6a, 0x6a, 0x6a));
    p.setColor(QPalette::Disabled, QPalette::Highlight, QColor(0x3c, 0x3c, 0x3c));
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, QColor(0x6a, 0x6a, 0x6a));
    return p;
}

const char *const kDarkStyleSheet =
    "QStatusBar { background-color: #2f2f30; color: #cccccc; border-top: 1px solid #3c3c3c; }"
    "QStatusBar::item { border: none; }"
    "QStatusBar QLabel { color: #cccccc; }"
    "QMenuBar { background-color: #3c3c3c; color: #cccccc; }"
    "QMenuBar::item { background-color: #3c3c3c; color: #cccccc; }"
    "QMenuBar::item:selected, QMenuBar::item:pressed { background-color: #094771; color: #ffffff; }"
    "QMenu { background-color: #252526; color: #cccccc; border: 1px solid #454545; }"
    "QMenu::item:selected { background-color: #094771; color: #ffffff; }"
    "QMenu::item:disabled { color: #6a6a6a; }"
    "QToolBar { background-color: #333333; border: none; spacing: 2px; }"
    "QDockWidget::title { background-color: #333333; padding: 3px; }";

} // namespace

QStringList ids()
{
    return { QStringLiteral("system"), QStringLiteral("dark") };
}

QString current()
{
    return g_current;
}

void apply(const QString &id)
{
    if (g_nativeStyle.isEmpty() && QApplication::style())
        g_nativeStyle = QApplication::style()->objectName();
    g_current = id == QLatin1String("vscode-dark") ? QStringLiteral("dark") // name of a test version
                                                    : ids().contains(id) ? id : defaultId();

    if (g_current == QLatin1String("system")) {
        QApplication::setStyle(QStyleFactory::create(g_nativeStyle));
        if (QApplication::style())
            QApplication::setPalette(QApplication::style()->standardPalette());
        qApp->setStyleSheet(QString());
        return;
    }
    // Fusion is the only style that follows a custom palette on every platform.
    QApplication::setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QApplication::setPalette(darkPalette());
    qApp->setStyleSheet(QLatin1String(kDarkStyleSheet));
}

bool isDark()
{
    return QApplication::palette().color(QPalette::Window).lightness() < 110;
}

QIcon toolbarIcon(const QString &name)
{
    QFile f(QStringLiteral(":/icons/toolbar/%1.svg").arg(name));
    if (!f.open(QIODevice::ReadOnly))
        return QIcon();
    QByteArray svg = f.readAll();
    if (isDark())
        svg.replace("#34495e", "#d6dde4"); // the dark slate of the icons, light on dark themes
    QSvgRenderer renderer(svg);
    QIcon icon;
    for (int size : { 16, 24, 32, 48, 64 }) {
        QPixmap pm(size, size);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        renderer.render(&p, QRectF(0, 0, size, size));
        p.end();
        icon.addPixmap(pm);
    }
    return icon;
}

} // namespace guitheme
