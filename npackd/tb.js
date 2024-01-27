// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

project.setVariable("NAME", "npackd");
project.setVariable("TYPE", "library");
project.setVariable("DEPENDENCIES", ["quazip1-qt5", "Qt5Sql", "Qt5Xml", "Qt5Core", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
system.include("..\\tb_common.js");
project.setVariable("QT_MOC_FILES", ["clprogress.h", "downloader.h", "installedpackages.h", "job.h"]);

project.findPkgConfigLibrary("Qt5Sql");
project.findPkgConfigLibrary("Qt5Xml");
project.findPkgConfigLibrary("Qt5Core");
project.findPkgConfigLibrary("quazip1-qt5");
if (project.getConfig() === "static")
    project.setLibraryPath("quazip1-qt5", "C:\\builds\\quazip_install\\lib\\libquazip1-qt5.a");
