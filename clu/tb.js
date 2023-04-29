// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

// TODO: src/app.rc.in
// TODO: src/app.ico
// TODO: qsqlite
// TODO: qtpcre2 zstd z
// TODO: ../LICENSE.txt
project.setName("clu");
project.setType("program");
project.setDependencies(["npackd", "quazip1-qt5", "Qt5Sql", "Qt5Xml", "Qt5Core", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
system.include("..\\tb_common.js");
