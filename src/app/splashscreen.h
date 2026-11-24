// SPDX-License-Identifier: GPL-3.0-or-later
// mbChess - start screen: the author's photo (as in the About dialog), name, version,
// what is being loaded and a progress bar. Can be switched off (View menu, setting
// ui/splash).
#pragma once

#include <QSplashScreen>

class SplashScreen : public QSplashScreen
{
    Q_OBJECT
public:
    SplashScreen();

    // percent 0..100; the text says what is loaded now
    void step(int percent, const QString &text);

    static bool enabled();
    static void setEnabled(bool on);

protected:
    void drawContents(QPainter *painter) override;

private:
    int m_percent = 0;
    QString m_text;
};

// The window reports its progress through this while the splash screen is shown.
namespace startup {
void setSplash(SplashScreen *splash);
void progress(int percent, const QString &text);
}
