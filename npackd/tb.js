// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackd");
    project.setVariable("TYPE", "library");

    var static = project.getConfig().indexOf("static") >= 0;

    var deps = ["Qt5Sql", "Qt5Xml", "Qt5Core", "userenv", "winmm", "ole32",
        "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd", "OleAut32"];
    if (static) {
        deps.push("quazip");
    } else {
        deps.push("quazip1-qt5");
    }
    project.setVariable("DEPENDENCIES", deps);

    system.include("..\\tb_common.js");
    project.setVariable("QT_MOC_FILES", ["clprogress.h", "downloader.h", "installedpackages.h", "job.h"]);

    project.findPkgConfigLibrary("Qt5Sql");
    project.findPkgConfigLibrary("Qt5Xml");
    project.findPkgConfigLibrary("Qt5Core");

    if (!static)
        project.findPkgConfigLibrary("quazip1-qt5");
}

