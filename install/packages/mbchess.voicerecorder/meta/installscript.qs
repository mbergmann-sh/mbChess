// mbChess installer: the voice recorder (optional). Start menu shortcut.
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
    component.addOperation("CreateShortcut", "@TargetDir@/mbchess-voicerecorder.exe",
                           "@StartMenuDir@/" + (de ? "mbChess-Sprachaufnahme" : "mbChess voice recorder") + ".lnk",
                           "workingDirectory=@TargetDir@", "iconPath=@TargetDir@/mbchess-voicerecorder.exe", "iconId=0",
                           "description=" + (de ? "Ansagen selbst einsprechen" : "Record your own announcements"));
}
