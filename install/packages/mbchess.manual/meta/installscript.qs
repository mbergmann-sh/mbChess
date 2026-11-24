// mbChess installer: the manual (PDF). Start menu shortcut to it.
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
    var file = installer.value("MbManualFile");
    if (!file)
        return;
    component.addOperation("CreateShortcut", "@TargetDir@/" + file,
                           "@StartMenuDir@/" + (de ? "mbChess-Handbuch" : "mbChess manual") + ".lnk",
                           "workingDirectory=@TargetDir@");
}
