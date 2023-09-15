// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

project.setName("npackd");
project.setType("library");
project.setDependencies(["quazip1-qt5.dll", "Qt5Sql", "Qt5Xml", "Qt5Core", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
system.include("..\\tb_common.js");
project.setQtMocFiles(["clprogress.h", "downloader.h", "installedpackages.h", "job.h"]);

project.findPkgConfigLibrary("Qt5Sql");
project.findPkgConfigLibrary("Qt5Xml");
project.findPkgConfigLibrary("Qt5Core");
//project.findPkgConfigLibrary("quazip1-qt5", "quazip1-qt5");
