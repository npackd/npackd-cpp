// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>
/// <reference path="../tb_common.js"/>

system.include("..\\tb_common.js");

function configure() {
    var static_ = project.getConfig().indexOf("static") >= 0;

    project.setVariable("NAME", "npackdg");
    project.setVariable("TITLE", "Npackd");
    project.setVariable("DESCRIPTION", "This is a Windows graphical user interface for the client to the Npackd package manager.");
    project.setVariable("TYPE", "program");
    project.setVariable("SUBSYSTEM", "windows");
    project.setVariable("MSI_UPGRADE_CODE", "EABC4270-7D7F-47BF-B388-AC63606E8DAC");

    var deps = ["npackd", "Qt" + qt_version + "Gui", "Qt" + qt_version + "Widgets", "Qt" + qt_version + "Svg", "gdi32"];
    if (static_) {
        deps.push("qwindows");
        deps.push("qmodernwindowsstyle");
        deps.push("qsqlite");
        deps.push("qicns");
        deps.push("qico");
        deps.push("qjpeg");
        deps.push("qgif");
        deps.push("qtiff");
        deps.push("qwbmp");
        deps.push("qwebp");
        deps.push("qsvg");
        deps.push("qtga");
    }
    project.setVariable("DEPENDENCIES", deps);

    project.setVariable("QT_MOC_FILES", ["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
        "licenseform.h", "mainframe.h", "mainwindow.h", "messageframe.h", "packageframe.h",
        "packageitemmodel.h", "packageversionform.h", "progresstree2.h", "repositoriesitemmodel.h",
        "visiblejobs.h", "settingsframe.h"]);

    setCommonFlags();

    // TODO: copy imageformats, iconengines?, platforms, sqldrivers and styles directories
    // for the build with shared libraries
}
