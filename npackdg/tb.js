// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    var static_ = project.getConfig().indexOf("static") >= 0;

    project.setVariable("NAME", "npackdg");
    project.setVariable("TITLE", "Npackd");
    project.setVariable("DESCRIPTION", "This is a Windows graphical user interface for the client to the Npackd package manager.");
    project.setVariable("TYPE", "program");
    project.setVariable("SUBSYSTEM", "windows");
    project.setVariable("MSI_UPGRADE_CODE", "EABC4270-7D7F-47BF-B388-AC63606E8DAC");

    var deps = ["npackd", "Qt5Gui", "Qt5Svg", "gdi32"];
    if (static_) {
        deps.push("qwindows");
        deps.push("qwindowsvistastyle");
        deps.push("qsqlite");
        deps.push("qicns");
        deps.push("qico");
        deps.push("qjpeg");
        deps.push("qgif");
        deps.push("qtiff");
        deps.push("qwbmp");
        deps.push("qwebp");
        deps.push("qsvg");
        deps.push("qtlibjpeg");
        deps.push("qtga");
    }
    project.setVariable("DEPENDENCIES", deps);

    project.setVariable("QT_MOC_FILES", ["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
        "licenseform.h", "mainframe.h", "mainwindow.h", "messageframe.h", "packageframe.h",
        "packageitemmodel.h", "packageversionform.h", "progresstree2.h", "repositoriesitemmodel.h",
        "visiblejobs.h", "settingsframe.h"]);

    system.include("..\\tb_common.js");
}
