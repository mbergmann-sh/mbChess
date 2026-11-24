# mbChess - builds the Windows installers (German and English) with the
# Qt Installer Framework.
# SPDX-License-Identifier: GPL-3.0-or-later
#
# Usage (after a release build in Qt Creator):
#   install\make-installer.bat  D:\Projekte\Qt5\mbChess\build\Desktop_Qt_6_12_0_MinGW_64_bit_Release
# or with qmake:  qmake CONFIG+=installer ... && mingw32-make   (runs this script last)
#
# Steps:
#   1. stage mbchess.exe and run windeployqt (Qt libraries, plugins, MinGW runtime)
#   2. stage the built UCI engines (bin\engines\<name>) with their licence files
#   3. stage the manual (docs\manual\*.pdf), if there is one
#   4. write config.xml and package.xml for each language
#   5. run binarycreator: install\output\mbChess-<version>-Setup-de.exe / -en.exe
#
# Parameters (all optional except the build folder, if it can't be found):
#   -BuildDir   build folder of mbChess (contains bin\mbchess.exe)
#   -QtBinDir   bin folder of the Qt kit (contains windeployqt.exe)
#   -IfwBinDir  bin folder of the Qt Installer Framework (contains binarycreator.exe)
#   -Languages  de,en (default both)

[CmdletBinding()]
param(
    [string]$BuildDir = "",
    [string]$QtBinDir = "",
    [string]$IfwBinDir = "",
    [string[]]$Languages = @("de", "en")
)

$ErrorActionPreference = "Stop"
$InstallDir = $PSScriptRoot
$Root = Split-Path -Parent $InstallDir
$Utf8 = New-Object System.Text.UTF8Encoding($false)   # UTF-8 without BOM

function Write-Step([string]$text) { Write-Host "mbChess-Installer: $text" -ForegroundColor Cyan }
function Fail([string]$text) { Write-Host "mbChess-Installer: FEHLER / ERROR: $text" -ForegroundColor Red; exit 1 }
function Write-Utf8([string]$path, [string]$text) { [System.IO.File]::WriteAllText($path, $text, $Utf8) }
function Read-Utf8([string]$path) { return [System.IO.File]::ReadAllText($path, [System.Text.Encoding]::UTF8) }
function Xml([string]$text) { return [System.Security.SecurityElement]::Escape($text) }
function New-Dir([string]$path) { New-Item -ItemType Directory -Force -Path $path | Out-Null }

# --- program version from common.pri ---------------------------------------
$common = Read-Utf8 (Join-Path $Root "common.pri")
if ($common -match "(?m)^\s*MBCHESS_VERSION\s*=\s*(\S+)") { $Version = $Matches[1] } else { Fail "MBCHESS_VERSION not found in common.pri" }
# (typographic quotes are built with [char]: PowerShell reads „ “ ” as double quotes)
$Codename = ""
if ($common -match "(?m)^\s*MBCHESS_CODENAME\s*=\s*(.+?)\s*$") { $Codename = $Matches[1] }
$ReleaseDate = Get-Date -Format "yyyy-MM-dd"

# --- build folder -----------------------------------------------------------
if ($BuildDir -eq "") {
    # newest build folder next to or inside the source folder
    $candidates = @(Get-ChildItem -Path @($Root, (Split-Path -Parent $Root)) -Directory -ErrorAction SilentlyContinue |
        ForEach-Object { Get-ChildItem -Path $_.FullName -Directory -ErrorAction SilentlyContinue } |
        Where-Object { Test-Path (Join-Path $_.FullName "bin\mbchess.exe") })
    $candidates += @(Get-ChildItem -Path @($Root, (Split-Path -Parent $Root)) -Directory -ErrorAction SilentlyContinue |
        Where-Object { Test-Path (Join-Path $_.FullName "bin\mbchess.exe") })
    if ($candidates.Count -eq 0) { Fail "no build folder with bin\mbchess.exe found; pass -BuildDir <folder>" }
    $BuildDir = ($candidates | Sort-Object { (Get-Item (Join-Path $_.FullName "bin\mbchess.exe")).LastWriteTime } -Descending |
        Select-Object -First 1).FullName
}
$BinDir = Join-Path $BuildDir "bin"
$Exe = Join-Path $BinDir "mbchess.exe"
if (-not (Test-Path $Exe)) { Fail "$Exe not found. Build mbChess (Release) first." }
Write-Step "Version $Version, build folder $BuildDir"

# --- tools ------------------------------------------------------------------
if ($QtBinDir -eq "") {
    $w = Get-Command windeployqt.exe -ErrorAction SilentlyContinue
    if ($w) { $QtBinDir = Split-Path -Parent $w.Source }
}
if ($QtBinDir -eq "") {
    # the Qt kit the build folder was made with (qmake writes it into the Makefile)
    $mk = Join-Path $BuildDir "Makefile"
    if (Test-Path $mk) {
        $m = Select-String -Path $mk -Pattern "^QMAKE\s*=\s*(.+qmake[^\s]*)" | Select-Object -First 1
        if ($m) { $QtBinDir = Split-Path -Parent ($m.Matches[0].Groups[1].Value.Trim()) }
    }
}
$WinDeployQt = Join-Path $QtBinDir "windeployqt.exe"
if (-not (Test-Path $WinDeployQt)) { Fail "windeployqt.exe not found; pass -QtBinDir <Qt>\6.x\mingw_64\bin" }

if ($IfwBinDir -eq "") {
    $b = Get-Command binarycreator.exe -ErrorAction SilentlyContinue
    if ($b) { $IfwBinDir = Split-Path -Parent $b.Source }
}
if ($IfwBinDir -eq "" -and $env:QTIFWDIR) { $IfwBinDir = Join-Path $env:QTIFWDIR "bin" }
if ($IfwBinDir -eq "") {
    # Qt Maintenance Tool installs it to <Qt>\Tools\QtInstallerFramework\<version>\bin
    $qtRoot = $QtBinDir
    for ($i = 0; $i -lt 3; $i++) { $qtRoot = Split-Path -Parent $qtRoot }
    foreach ($base in @($qtRoot, "C:\Qt")) {
        $ifw = Get-ChildItem -Path (Join-Path $base "Tools\QtInstallerFramework") -Directory -ErrorAction SilentlyContinue |
            Sort-Object { try { [version]$_.Name } catch { [version]"0.0" } } -Descending | Select-Object -First 1
        if ($ifw) { $IfwBinDir = Join-Path $ifw.FullName "bin"; break }
    }
}
$BinaryCreator = Join-Path $IfwBinDir "binarycreator.exe"
if (-not (Test-Path $BinaryCreator)) {
    Fail "binarycreator.exe not found. Install the Qt Installer Framework (Qt Maintenance Tool > Developer and Designer Tools) or pass -IfwBinDir"
}

# windeployqt finds the MinGW runtime through PATH (g++.exe)
$env:PATH = "$QtBinDir;$env:PATH"
if (-not (Get-Command g++.exe -ErrorAction SilentlyContinue)) {
    $qtRoot = $QtBinDir
    for ($i = 0; $i -lt 3; $i++) { $qtRoot = Split-Path -Parent $qtRoot }
    $mingw = Get-ChildItem -Path (Join-Path $qtRoot "Tools") -Directory -Filter "mingw*" -ErrorAction SilentlyContinue |
        Sort-Object Name -Descending | Select-Object -First 1
    if ($mingw) { $env:PATH = (Join-Path $mingw.FullName "bin") + ";$env:PATH" }
}

# --- staging folder (rebuilt every time) ------------------------------------
$Stage = Join-Path $InstallDir "build"
$Output = Join-Path $InstallDir "output"
if (Test-Path $Stage) { Remove-Item -Recurse -Force $Stage }
New-Dir $Stage
New-Dir $Output

# 1. program + Qt ------------------------------------------------------------
Write-Step "windeployqt"
$AppData = Join-Path $Stage "app"
New-Dir $AppData
Copy-Item $Exe $AppData
& $WinDeployQt --compiler-runtime --no-quick-import (Join-Path $AppData "mbchess.exe")
if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed ($LASTEXITCODE)" }
foreach ($doc in @("LICENSE", "README.md", "CREDITS.md")) {
    $src = Join-Path $Root $doc
    if (Test-Path $src) { Copy-Item $src $AppData }
}
if (Test-Path (Join-Path $AppData "LICENSE")) { Rename-Item (Join-Path $AppData "LICENSE") "LICENSE.txt" }

# voice recorder (optional component): its own windeployqt run, then only the files
# that mbchess.exe does not bring already (Qt Multimedia and its plugins)
$RecorderExe = Join-Path $BinDir "mbchess-voicerecorder.exe"
$RecorderData = Join-Path $Stage "voicerecorder"
$HaveRecorder = $false
if (Test-Path $RecorderExe) {
    Write-Step "windeployqt (voice recorder)"
    $full = Join-Path $Stage "voicerecorder-full"
    New-Dir $full
    New-Dir $RecorderData
    Copy-Item $RecorderExe $full
    & $WinDeployQt --compiler-runtime --no-quick-import --no-translations (Join-Path $full "mbchess-voicerecorder.exe")
    if ($LASTEXITCODE -ne 0) { Fail "windeployqt failed for the voice recorder ($LASTEXITCODE)" }
    foreach ($f in Get-ChildItem -Path $full -Recurse -File) {
        $rel = $f.FullName.Substring($full.Length + 1)
        if (Test-Path (Join-Path $AppData $rel)) { continue }
        $dest = Join-Path $RecorderData $rel
        New-Dir (Split-Path -Parent $dest)
        Copy-Item $f.FullName $dest
    }
    $HaveRecorder = $true
} else {
    Write-Host "mbChess-Installer: no $RecorderExe - the installer gets no voice recorder" -ForegroundColor Yellow
}

# folders for opening books, chessbooks and tablebases, see src/app/datafolders.h
$readme = @{
    "Openings"   = @("Eröffnungsbücher im PolyGlot-Format (*.bin) hierher kopieren.",
                     "Copy opening books in PolyGlot format (*.bin) here.")
    "Chessbooks" = @("Partiesammlungen (*.pgn) hierher kopieren. mbChess baut daraus ein Buch, wie Crafty.",
                     "Copy game collections (*.pgn) here. mbChess builds a book from them, like Crafty does.")
    "Syzygy"     = @("Endspiel-Datenbanken (Syzygy, *.rtbw/*.rtbz). mbChess lädt sie über Engines > Endspiel-Datenbanken herunter.",
                     "Endgame tablebases (Syzygy, *.rtbw/*.rtbz). mbChess downloads them with Engines > Endgame tablebases.")
}
foreach ($folder in $readme.Keys) {
    New-Dir (Join-Path $AppData $folder)
    Write-Utf8 (Join-Path $AppData "$folder\LIESMICH.txt") ($readme[$folder][0] + "`r`n")
    Write-Utf8 (Join-Path $AppData "$folder\README.txt") ($readme[$folder][1] + "`r`n")
}

# 2. engines -------------------------------------------------------------------
$engineNames = @{
    "stockfish" = @("Stockfish", "GPL-3.0")
    "arasan"    = @("Arasan", "MIT")
    "fruit"     = @("Fruit 2.1", "GPL-2.0-or-later")
    "toga"      = @("Toga II", "GPL-2.0-or-later")
    "glaurung"  = @("Glaurung 2.2", "GPL-3.0-or-later")
    "gnuchess"  = @("GNU Chess", "GPL-3.0-or-later")
    "viridithas"= @("Viridithas", "AGPL-3.0")
}
$engines = @()
$engineRoot = Join-Path $BinDir "engines"
if (Test-Path $engineRoot) {
    foreach ($dir in Get-ChildItem -Path $engineRoot -Directory) {
        if (-not (Get-ChildItem -Path $dir.FullName -Filter "*.exe" -ErrorAction SilentlyContinue)) { continue }
        # Crafty: licence allows personal use only, no redistribution - never in the installer
        if ($dir.Name -ieq "crafty") { Write-Step "engines: crafty skipped (not redistributable)"; continue }
        $engines += $dir.Name
    }
}
if ($engines.Count -eq 0) { Write-Host "mbChess-Installer: no built engines in $engineRoot - the installer gets no engines" -ForegroundColor Yellow }
else { Write-Step ("engines: " + ($engines -join ", ")) }

# 3. manual -----------------------------------------------------------------------
function Find-Manual([string]$lang) {
    $dir = Join-Path $Root "docs\manual"
    $wanted = if ($lang -eq "de") { "mbChess-Handbuch.pdf" } else { "mbChess-Manual.pdf" }
    if (Test-Path (Join-Path $dir $wanted)) { return Join-Path $dir $wanted }
    $any = Get-ChildItem -Path $dir -Filter "*.pdf" -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($any) { return $any.FullName }
    return ""
}

# texts per language ----------------------------------------------------------------
$Texts = @{  # not $T: PowerShell names ignore case, $t below would overwrite it
    de = @{
        title = $(if ($Codename) { "mbChess $Version " + [char]0x201E + $Codename + [char]0x201C + " installieren" } else { "mbChess $Version installieren" }); runDesc = "mbChess jetzt starten"
        tool = "mbChess-Deinstallation"
        app = "mbChess"; appDesc = "Das Schachprogramm mit Brettern, Figuren, Klängen und Eröffnungsbuch."
        engines = "UCI-Schach-Engines"; enginesDesc = "Fertig gebaute Schach-Engines als Gegner und für die Analyse."
        engineDesc = "UCI-Engine {0} (Lizenz: {1})"
        manual = "Handbuch (PDF)"; manualDesc = "Das Benutzerhandbuch von mbChess als PDF-Datei."
        recorder = "Sprachaufnahme"; recorderDesc = "Werkzeug, um die Ansagen (Züge, Begrüßung) selbst einzusprechen."
        manualDir = "Handbuch"; license = "Lizenz von mbChess (GNU GPL 3)"; engineLicense = "Lizenz von {0}"
    }
    en = @{
        title = $(if ($Codename) { "Install mbChess $Version `"$Codename`"" } else { "Install mbChess $Version" }); runDesc = "Start mbChess now"
        tool = "mbChess-Uninstall"
        app = "mbChess"; appDesc = "The chess program with boards, pieces, sounds and opening book."
        engines = "UCI chess engines"; enginesDesc = "Ready-built chess engines to play against and for analysis."
        engineDesc = "UCI engine {0} (licence: {1})"
        manual = "Manual (PDF)"; manualDesc = "The mbChess user manual as a PDF file."
        recorder = "Voice recorder"; recorderDesc = "Tool to record the announcements (moves, greeting) in your own voice."
        manualDir = "Manual"; license = "mbChess licence (GNU GPL 3)"; engineLicense = "{0} licence"
    }
}

function Write-Package([string]$packages, [string]$name, [string]$display, [string]$desc, [string]$default,
                       [bool]$forced, [int]$sorting, [string]$extra) {
    $meta = Join-Path $packages "$name\meta"
    New-Dir $meta
    New-Dir (Join-Path $packages "$name\data")
    $xml = @"
<?xml version="1.0" encoding="UTF-8"?>
<Package>
    <DisplayName>$(Xml $display)</DisplayName>
    <Description>$(Xml $desc)</Description>
    <Version>$Version</Version>
    <ReleaseDate>$ReleaseDate</ReleaseDate>
    <Default>$default</Default>
    <ForcedInstallation>$(if ($forced) { "true" } else { "false" })</ForcedInstallation>
    <SortingPriority>$sorting</SortingPriority>
$extra</Package>
"@
    Write-Utf8 (Join-Path $meta "package.xml") $xml
    return $meta
}

$gpl = Read-Utf8 (Join-Path $Root "LICENSE")
$ControlTemplate = Read-Utf8 (Join-Path $InstallDir "config\controlscript.qs")

foreach ($lang in $Languages) {
    if (-not $Texts.ContainsKey($lang)) { Fail "unknown language $lang (de, en)" }
    $t = $Texts[$lang]
    Write-Step "installer ($lang)"
    $LangDir = Join-Path $Stage $lang
    $config = Join-Path $LangDir "config"
    $packages = Join-Path $LangDir "packages"
    New-Dir $config
    New-Dir $packages

    # --- the program (always installed)
    $licenseFile = "license_$lang.txt"
    $meta = Write-Package $packages "mbchess.app" $t.app $t.appDesc "true" $true 100 @"
    <Script>installscript.qs</Script>
    <Licenses>
        <License name="$(Xml $t.license)" file="$licenseFile" priority="100"/>
    </Licenses>

"@
    Copy-Item (Join-Path $InstallDir "packages\mbchess.app\meta\installscript.qs") $meta
    Write-Utf8 (Join-Path $meta $licenseFile) ((Read-Utf8 (Join-Path $InstallDir "license\$licenseFile")) + $gpl)
    Copy-Item -Recurse (Join-Path $AppData "*") (Join-Path $packages "mbchess.app\data")
    # the help (F1) belongs to the program: the .chm of this language, built by tools/build-manual.py
    $chmName = if ($lang -eq "de") { "mbChess-Handbuch.chm" } else { "mbChess-Manual.chm" }
    $chm = Join-Path $Root "docs\manual\$chmName"
    $helpTarget = ""
    if (Test-Path $chm) {
        $helpTarget = "$($t.manualDir)/$chmName"
        $helpDir = Join-Path $packages "mbchess.app\data\$($t.manualDir)"
        New-Dir $helpDir
        Copy-Item $chm $helpDir
    } else {
        Write-Host "mbChess-Installer: no docs\manual\$chmName - F1 opens the PDF (if installed)" -ForegroundColor Yellow
    }

    # --- engines (optional, default: yes); one sub-component per engine
    if ($engines.Count -gt 0) {
        Write-Package $packages "mbchess.engines" $t.engines $t.enginesDesc "true" $false 90 "" | Out-Null
        $sort = 50
        foreach ($e in $engines) {
            $info = if ($engineNames.ContainsKey($e)) { $engineNames[$e] } else { @($e, "") }
            $licSrc = Get-ChildItem -Path (Join-Path $Root "engines\$e") -File -ErrorAction SilentlyContinue |
                Where-Object { $_.Name -match "^(copying|licen[cs]e)" } | Select-Object -First 1
            $extra = ""
            if ($licSrc) {
                $extra = "    <Licenses>`r`n        <License name=`"$(Xml ($t.engineLicense -f $info[0]))`" file=`"license.txt`"/>`r`n    </Licenses>`r`n"
            }
            $meta = Write-Package $packages "mbchess.engines.$e" $info[0] ($t.engineDesc -f $info[0], $info[1]) "true" $false $sort $extra
            $sort--
            $data = Join-Path $packages "mbchess.engines.$e\data\engines\$e"
            New-Dir $data
            Copy-Item -Recurse (Join-Path $engineRoot "$e\*") $data
            if ($licSrc) {
                Copy-Item $licSrc.FullName (Join-Path $meta "license.txt")
                Copy-Item $licSrc.FullName (Join-Path $data "LICENSE-$e.txt")
            }
        }
    }

    # --- voice recorder (optional, default: no)
    if ($HaveRecorder) {
        $meta = Write-Package $packages "mbchess.voicerecorder" $t.recorder $t.recorderDesc "false" $false 85 "    <Script>installscript.qs</Script>`r`n"
        Copy-Item (Join-Path $InstallDir "packages\mbchess.voicerecorder\meta\installscript.qs") $meta
        $data = Join-Path $packages "mbchess.voicerecorder\data"
        New-Dir $data
        Copy-Item -Recurse (Join-Path $RecorderData "*") $data
    }

    # --- manual (optional, default: no)
    $manual = Find-Manual $lang
    $manualTarget = ""
    if ($manual -ne "") {
        $meta = Write-Package $packages "mbchess.manual" $t.manual $t.manualDesc "false" $false 80 "    <Script>installscript.qs</Script>`r`n"
        Copy-Item (Join-Path $InstallDir "packages\mbchess.manual\meta\installscript.qs") $meta
        $data = Join-Path $packages "mbchess.manual\data\$($t.manualDir)"
        New-Dir $data
        Copy-Item $manual $data
        $manualTarget = "$($t.manualDir)/" + (Split-Path -Leaf $manual)
    } elseif ($lang -eq $Languages[0]) {
        Write-Host "mbChess-Installer: no manual in docs\manual\*.pdf - the installer gets no manual" -ForegroundColor Yellow
    }

    # --- config.xml + control script
    Copy-Item (Join-Path $InstallDir "images\watermark.png") $config
    Copy-Item (Join-Path $InstallDir "images\logo.png") $config
    Copy-Item (Join-Path $Root "src\app\resources\icons\mbchess.ico") $config
    Copy-Item (Join-Path $Root "src\app\resources\icons\mbchess.png") $config
    # the photo for the welcome page, inline (the resource folder names differ between QtIFW versions).
    # PNG: QtIFW has no JPEG plugin, a JPEG shows only a broken-image symbol.
    $photo = [Convert]::ToBase64String([IO.File]::ReadAllBytes((Join-Path $InstallDir "images\photo.png")))
    $script = "var MB_LANG = `"$lang`";`r`nvar MB_MANUAL = `"$manualTarget`";`r`nvar MB_HELP = `"$helpTarget`";`r`n" +
              "var MB_PHOTO = `"data:image/png;base64,$photo`";`r`n" + $ControlTemplate.Replace("@VERSION@", $Version)
    Write-Utf8 (Join-Path $config "controlscript.qs") $script
    $configXml = @"
<?xml version="1.0" encoding="UTF-8"?>
<Installer>
    <Name>mbChess</Name>
    <Version>$Version</Version>
    <Title>$(Xml $t.title)</Title>
    <Publisher>Michael Bergmann</Publisher>
    <ProductUrl>https://github.com/mbergmann-sh/mbChess</ProductUrl>
    <InstallerApplicationIcon>mbchess</InstallerApplicationIcon>
    <InstallerWindowIcon>mbchess.png</InstallerWindowIcon>
    <Logo>logo.png</Logo>
    <Watermark>watermark.png</Watermark>
    <WizardStyle>Modern</WizardStyle>
    <WizardDefaultWidth>820</WizardDefaultWidth>
    <WizardDefaultHeight>520</WizardDefaultHeight>
    <StartMenuDir>mbChess</StartMenuDir>
    <TargetDir>C:/mbChess</TargetDir>
    <AdminTargetDir>C:/mbChess</AdminTargetDir>
    <AllowSpaceInPath>true</AllowSpaceInPath>
    <AllowNonAsciiCharacters>true</AllowNonAsciiCharacters>
    <RemoveTargetDir>false</RemoveTargetDir>
    <MaintenanceToolName>$($t.tool)</MaintenanceToolName>
    <RunProgram>@TargetDir@/mbchess.exe</RunProgram>
    <RunProgramDescription>$(Xml $t.runDesc)</RunProgramDescription>
    <ControlScript>controlscript.qs</ControlScript>
</Installer>
"@
    Write-Utf8 (Join-Path $config "config.xml") $configXml

    # --- binarycreator
    $setup = Join-Path $Output "mbChess-$Version-Setup-$lang.exe"
    if (Test-Path $setup) { Remove-Item -Force $setup }
    & $BinaryCreator --offline-only -c (Join-Path $config "config.xml") -p $packages $setup
    if ($LASTEXITCODE -ne 0) { Fail "binarycreator failed ($LASTEXITCODE)" }
    Write-Step "fertig / done: $setup"
}
