// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackd");
    project.setVariable("TYPE", "library");

    var static_ = project.getConfig().indexOf("static") >= 0;

    var deps = ["Qt5Sql", "Qt5Xml", "Qt5Core", "userenv", "winmm", "ole32",
        "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd", "OleAut32"];
    if (static_) {
        deps.push("quazip");
        deps.push("qsqlite");
   } else {
        deps.push("quazip1-qt5");
    }
    project.setVariable("DEPENDENCIES", deps);

    system.include("..\\tb_common.js");
    project.setVariable("QT_MOC_FILES", ["clprogress.h", "downloader.h", "installedpackages.h", "job.h"]);

    project.findPkgConfigLibrary("Qt5Sql", { static: static_ });
    project.findPkgConfigLibrary("Qt5Xml", { static: static_ });
    project.findPkgConfigLibrary("Qt5Core", { static: static_ });

    if (!static_) {
        project.findPkgConfigLibrary("quazip1-qt5", { static: static_ });

        var qt = "C:\\msys64\\mingw64\\qt5-static\\";
        var qt_plugins = qt + "share\\qt5\\plugins\\";
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");
    }
}

