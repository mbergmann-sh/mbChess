@echo off
rem mbChess - builds the Windows installers (German and English) with the
rem Qt Installer Framework. Details: install\README.md and make-installer.ps1
rem
rem   install\make-installer.bat  [build folder]  [more options for make-installer.ps1]
rem
rem Example:
rem   install\make-installer.bat D:\Projekte\Qt5\mbChess\build\Desktop_Qt_6_12_0_MinGW_64_bit_Release

set "BUILD=%~1"
if "%BUILD:~0,1%"=="-" set "BUILD="
if not "%BUILD%"=="" shift
if "%BUILD%"=="" (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-installer.ps1" %1 %2 %3 %4 %5 %6
) else (
    powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0make-installer.ps1" -BuildDir "%BUILD%" %1 %2 %3 %4 %5 %6
)
exit /b %ERRORLEVEL%
