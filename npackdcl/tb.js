// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

// TODO: src/app.rc.in
// TODO: src/app.ico
// TODO: npackdcl.rc
// TODO: qsqlite
// TODO: qtpcre2 zstd z
// TODO: ../CrystalIcons_LICENSE.txt ../LICENSE.txt 
// TODO: npackdcl.manifest
project.setName("npackdcl");
project.setType("program");
project.setDependencies(["npackd", "quazip1-qt5", "Qt5Sql", "Qt5Xml", "Qt5Core", "oleaut32", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
project.setQtMocFiles(["app.h"]);
system.include("..\\tb_common.js");

project.findPkgConfigLibrary("Qt5Sql");
project.findPkgConfigLibrary("Qt5Xml");
project.findPkgConfigLibrary("Qt5Core");
project.findPkgConfigLibrary("quazip1-qt5");
