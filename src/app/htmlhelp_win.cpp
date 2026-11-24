// SPDX-License-Identifier: GPL-3.0-or-later
#include "htmlhelp.h"

#include <QDir>

#include <qt_windows.h>

#include <string>

namespace {
// HtmlHelpW from hhctrl.ocx, loaded at run time (MinGW has no import library for it)
using HtmlHelpFunc = HWND(WINAPI *)(HWND, LPCWSTR, UINT, DWORD_PTR);
constexpr UINT HhDisplayTopic = 0x0000;
constexpr UINT HhCloseAll = 0x0012;

HtmlHelpFunc htmlHelpFunc()
{
    static const HtmlHelpFunc func = [] {
        HMODULE lib = LoadLibraryW(L"hhctrl.ocx");
        return lib ? reinterpret_cast<HtmlHelpFunc>(reinterpret_cast<void *>(GetProcAddress(lib, "HtmlHelpW")))
                   : HtmlHelpFunc(nullptr);
    }();
    return func;
}

bool used = false;
} // namespace

namespace htmlhelp {

bool show(const QString &chmFile)
{
    const HtmlHelpFunc func = htmlHelpFunc();
    if (!func)
        return false;
    const std::wstring path = QDir::toNativeSeparators(chmFile).toStdWString();
    // No owner window: the help may stay beside mbChess and is not minimised with it.
    HWND help = func(nullptr, path.c_str(), HhDisplayTopic, 0);
    if (!help)
        return false;
    used = true;
    if (IsIconic(help))
        ShowWindow(help, SW_RESTORE);
    SetForegroundWindow(help);
    return true;
}

void closeAll()
{
    if (used && htmlHelpFunc())
        htmlHelpFunc()(nullptr, nullptr, HhCloseAll, 0);
    used = false;
}

} // namespace htmlhelp
