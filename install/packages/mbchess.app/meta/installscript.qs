// mbChess installer: the program itself. Start menu and desktop shortcuts.
// SPDX-License-Identifier: GPL-3.0-or-later

function Component()
{
}

Component.prototype.createOperations = function()
{
    component.createOperations();
    if (systemInfo.productType !== "windows")
        return;
    var de = installer.value("MbLanguage") === "de";
    component.addOperation("CreateShortcut", "@TargetDir@/mbchess.exe", "@StartMenuDir@/mbChess.lnk",
                           "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/mbchess.exe", "iconId=0",
                           "description=" + (de ? "Schach spielen mit mbChess" : "Play chess with mbChess"));
    component.addOperation("CreateShortcut", "@TargetDir@/mbchess.exe", "@DesktopDir@/mbChess.lnk",
                           "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/mbchess.exe", "iconId=0",
                           "description=" + (de ? "Schach spielen mit mbChess" : "Play chess with mbChess"));
    var help = installer.value("MbHelpFile"); // the help (CHM) that F1 opens
    if (help)
        component.addOperation("CreateShortcut", "@TargetDir@/" + help,
                               "@StartMenuDir@/" + (de ? "mbChess-Hilfe" : "mbChess help") + ".lnk",
                               "workingDirectory=@TargetDir@");
    component.addOperation("CreateShortcut", "@TargetDir@/@MaintenanceToolName@.exe",
                           "@StartMenuDir@/" + (de ? "mbChess deinstallieren" : "Uninstall mbChess") + ".lnk",
                           "workingDirectory=@TargetDir@",
                           "description=" + (de ? "mbChess entfernen" : "Remove mbChess"));
}
