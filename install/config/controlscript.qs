// mbChess installer: control script (Qt Installer Framework 4.x)
// SPDX-License-Identifier: GPL-3.0-or-later
//
// make-installer.ps1 writes these lines in front of this file, so that each
// installer shows its texts in one language:
//   var MB_LANG = "de";                                  (or "en")
//   var MB_MANUAL = "Handbuch/mbChess-Handbuch.pdf";     ("" without manual)
//   var MB_HELP = "Handbuch/mbChess-Handbuch.chm";       (the help for F1, "" without it)
//   var MB_PHOTO = "data:image/png;base64,...";          (images/photo.png, the About picture)
// "@VERSION@" below is replaced with the program version.

var TEXTS = {
    de: {
        welcome: "<h2>Willkommen bei mb-Chess,</h2>"
               + "<p>dem vielseitigen Schachprogramm für Windows, Linux, MacOS und Android.</p>"
               + "<p>Dieser Assistent installiert mbChess @VERSION@ auf Ihrem Computer. "
               + "Auf Wunsch werden auch die mitgelieferten UCI-Schach-Engines und das Handbuch (PDF) installiert.</p>"
               + "<p>mbChess ist freie Software unter der GNU General Public License, Version 3 oder später. "
               + "Es wird ohne jede Gewährleistung geliefert. Den Lizenztext zeigt Ihnen der Assistent gleich.</p>"
               + "<p>© Michael Bergmann 2026</p>"
               + "<p>Klicken Sie auf „Weiter“, um fortzufahren, oder auf „Abbrechen“, um den Assistenten zu beenden.</p>",
        replace: "<p><b>Hinweis:</b> In diesem Ordner ist mbChess bereits installiert. "
               + "Die vorhandene Installation wird ersetzt. Eigene Dateien in „Openings“ und „Chessbooks“ bleiben erhalten.</p>"
    },
    en: {
        welcome: "<h2>Welcome to mb-Chess,</h2>"
               + "<p>the versatile chess program for Windows, Linux, MacOS and Android.</p>"
               + "<p>This wizard installs mbChess @VERSION@ on your computer. "
               + "If you like, it also installs the bundled UCI chess engines and the manual (PDF).</p>"
               + "<p>mbChess is free software under the GNU General Public License, version 3 or later. "
               + "It comes without any warranty. The wizard shows you the licence text next.</p>"
               + "<p>© Michael Bergmann 2026</p>"
               + "<p>Click \"Next\" to continue or \"Cancel\" to leave the wizard.</p>",
        replace: "<p><b>Note:</b> mbChess is already installed in this folder. "
               + "The existing installation will be replaced. Your own files in \"Openings\" and \"Chessbooks\" are kept.</p>"
    }
};
var T = TEXTS[MB_LANG] || TEXTS.en;

// Names of the uninstall program of earlier mbChess installations (both languages)
var OLD_TOOLS = ["mbChess-Deinstallation", "mbChess-Uninstall", "maintenancetool"];

function oldMaintenanceTool()
{
    var dir = installer.value("TargetDir");
    if (!dir)
        return "";
    var ext = systemInfo.productType === "windows" ? ".exe" : "";
    for (var i = 0; i < OLD_TOOLS.length; ++i) {
        var path = dir + "/" + OLD_TOOLS[i] + ext;
        if (installer.fileExists(path))
            return path;
    }
    return "";
}

// An existing installation in the target folder is removed silently before the
// new files are copied ("purge" uninstalls everything without questions).
// RemoveTargetDir is false, so the folder and the user's own files stay.
function removeOldInstallation()
{
    var tool = oldMaintenanceTool();
    if (tool === "")
        return;
    console.log("mbChess: removing the existing installation with " + tool);
    installer.execute(tool, ["purge", "-c"]);
}

function Controller()
{
    // read by the component scripts (shortcut names)
    installer.setValue("MbLanguage", MB_LANG);
    installer.setValue("MbManualFile", MB_MANUAL);
    installer.setValue("MbHelpFile", (typeof MB_HELP === "string") ? MB_HELP : "");
    installer.installationStarted.connect(removeOldInstallation);
}

// The photo (the About picture, small) sits on the right of the welcome text; the
// list of installer steps stays on the left. binarycreator stores the files of the
// config folder as resources; the folder name differs between QtIFW versions, so the
// first one that exists is used, else the inline copy MB_PHOTO.
var PHOTO_PATHS = [":/metadata/installer-config/watermark.png", ":/config/watermark.png",
                   ":/metadata/watermark.png", ":/watermark.png"];

function photoPath()
{
    for (var i = 0; i < PHOTO_PATHS.length; ++i)
        if (installer.fileExists(PHOTO_PATHS[i]))
            return PHOTO_PATHS[i];
    // otherwise the copy that make-installer.ps1 puts into this script
    return (typeof MB_PHOTO === "string") ? MB_PHOTO : "";
}

function withPhoto(html)
{
    var photo = photoPath();
    if (photo === "")
        return html;
    return "<table cellspacing=\"0\" cellpadding=\"0\"><tr>"
         + "<td valign=\"top\">" + html + "</td>"
         + "<td width=\"16\"></td>"
         + "<td valign=\"top\"><img src=\"" + photo + "\" width=\"160\"></td>"
         + "</tr></table>";
}

Controller.prototype.IntroductionPageCallback = function()
{
    var page = gui.currentPageWidget();
    if (page !== null && page.MessageLabel)
        page.MessageLabel.setText(withPhoto(T.welcome));
}

Controller.prototype.ReadyForInstallationPageCallback = function()
{
    var page = gui.currentPageWidget();
    if (page === null || !page.MessageLabel)
        return;
    if (oldMaintenanceTool() !== "")
        page.MessageLabel.setText(page.MessageLabel.text + T.replace);
}
