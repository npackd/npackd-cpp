// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

project.setName("tests");
project.setType("program");
project.setDependencies(["npackd", "quazip1-qt5", "Qt5Sql", "Qt5Xml", "Qt5Test", "Qt5Core", "oleaut32", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
project.setQtMocFiles(["app.h"]);
system.include("..\\tb_common.js");

project.findPkgConfigLibrary("Qt5Sql");
project.findPkgConfigLibrary("Qt5Xml");
project.findPkgConfigLibrary("Qt5Core");
project.findPkgConfigLibrary("Qt5Test");
project.findPkgConfigLibrary("quazip1-qt5");
