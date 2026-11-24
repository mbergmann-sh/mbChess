// SPDX-License-Identifier: GPL-3.0-or-later
#include "splashscreen.h"

#include "mbchess_codename.h"

#include <QApplication>
#include <QDate>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>
#include <QPointer>
#include <QScreen>
#include <QSettings>

namespace {
const int kWidth = 640;
const int kHeight = 360;
const int kPhotoWidth = 290;
QPointer<SplashScreen> g_splash;
} // namespace

SplashScreen::SplashScreen()
{
    const qreal dpr = qApp->primaryScreen() ? qApp->primaryScreen()->devicePixelRatio() : 1.0;
    QPixmap pm(QSize(kWidth, kHeight) * dpr);
    pm.setDevicePixelRatio(dpr);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::SmoothPixmapTransform);
    QLinearGradient bg(0, 0, kWidth, kHeight);
    bg.setColorAt(0, QColor(0x22, 0x2f, 0x3c));
    bg.setColorAt(1, QColor(0x34, 0x49, 0x5e));
    p.fillRect(QRect(0, 0, kWidth, kHeight), bg);

    // photo on the left, cut to the frame
    QImage photo(QStringLiteral(":/images/about.jpg"));
    if (!photo.isNull()) {
        photo = photo.scaledToHeight(qRound(kHeight * dpr), Qt::SmoothTransformation);
        const int w = qMin(photo.width(), qRound(kPhotoWidth * dpr));
        photo = photo.copy((photo.width() - w) / 2, 0, w, photo.height());
        photo.setDevicePixelRatio(dpr);
        p.drawImage(QPointF(0, 0), photo);
    }
    // soft edge between photo and text
    QLinearGradient edge(kPhotoWidth - 40, 0, kPhotoWidth, 0);
    edge.setColorAt(0, QColor(0x22, 0x2f, 0x3c, 0));
    edge.setColorAt(1, QColor(0x22, 0x2f, 0x3c, 255));
    p.fillRect(QRectF(kPhotoWidth - 40, 0, 40, kHeight), edge);

    const int x = kPhotoWidth + 24;
    QFont title = qApp->font();
    title.setPixelSize(44);
    title.setBold(true);
    p.setFont(title);
    p.setPen(Qt::white);
    p.drawText(QRect(x, 40, kWidth - x - 20, 56), Qt::AlignLeft | Qt::AlignVCenter, QStringLiteral("mbChess"));
    QFont sub = qApp->font();
    sub.setPixelSize(15);
    p.setFont(sub);
    p.setPen(QColor(0xd6, 0xdd, 0xe4));
    p.drawText(QRect(x, 100, kWidth - x - 20, 44), Qt::AlignLeft | Qt::TextWordWrap,
               tr("The versatile chess program for Windows, Linux, macOS and Android"));
    p.setPen(QColor(0xa9, 0xb7, 0xc4));
    p.drawText(QRect(x, 156, kWidth - x - 20, 22), Qt::AlignLeft,
               tr("Version %1 \"%2\"").arg(QApplication::applicationVersion(), QStringLiteral(MBCHESS_CODENAME)));
    p.drawText(QRect(x, 178, kWidth - x - 20, 22), Qt::AlignLeft,
               QStringLiteral("© Michael Bergmann 2026"));
    p.end();
    setPixmap(pm);
}

bool SplashScreen::enabled()
{
    return QSettings().value(QStringLiteral("ui/splash"), true).toBool();
}

void SplashScreen::setEnabled(bool on)
{
    QSettings().setValue(QStringLiteral("ui/splash"), on);
}

void SplashScreen::step(int percent, const QString &text)
{
    m_percent = qBound(0, percent, 100);
    m_text = text;
    showMessage(text); // repaints
    qApp->processEvents();
}

void SplashScreen::drawContents(QPainter *p)
{
    p->setRenderHint(QPainter::Antialiasing);
    const int x = kPhotoWidth + 24;
    const int w = kWidth - x - 24;
    QFont f = qApp->font();
    f.setPixelSize(13);
    p->setFont(f);
    p->setPen(QColor(0xd6, 0xdd, 0xe4));
    p->drawText(QRect(x, kHeight - 66, w, 20), Qt::AlignLeft | Qt::AlignVCenter,
                p->fontMetrics().elidedText(m_text, Qt::ElideRight, w));
    const QRectF bar(x, kHeight - 40, w, 10);
    QPainterPath frame;
    frame.addRoundedRect(bar, 5, 5);
    p->fillPath(frame, QColor(255, 255, 255, 40));
    if (m_percent > 0) {
        QPainterPath fill;
        fill.addRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * m_percent / 100.0, bar.height()), 5, 5);
        QLinearGradient g(bar.topLeft(), bar.topRight());
        g.setColorAt(0, QColor(0x3d, 0x8e, 0xd8));
        g.setColorAt(1, QColor(0x5d, 0xc8, 0x8a));
        p->fillPath(fill, g);
    }
}

namespace startup {

void setSplash(SplashScreen *splash)
{
    g_splash = splash;
}

void progress(int percent, const QString &text)
{
    if (g_splash)
        g_splash->step(percent, text);
}

} // namespace startup
