// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    // TODO: src/app.rc.in
    // TODO: src/app.ico
    // TODO: qsqlite
    // TODO: qtpcre2 zstd z
    // TODO: ../LICENSE.txt
    project.setVariable("NAME", "clu");
    project.setVariable("TYPE", "program");
    project.setVariable("DEPENDENCIES",
        ["npackd", "quazip1-qt5", "Qt5Sql", "Qt5Xml", "Qt5Core", "oleaut32", "userenv", "winmm", "ole32",
            "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);
    system.include("..\\tb_common.js");

    var static = project.getConfig().indexOf("static") >= 0;

    project.findPkgConfigLibrary("Qt5Sql", { static: static });
    project.findPkgConfigLibrary("Qt5Xml", { static: static });
    project.findPkgConfigLibrary("Qt5Core", { static: static });
    project.findPkgConfigLibrary("quazip1-qt5");

    if (static)
        project.setLibraryPath("quazip1-qt5", "C:\\builds\\quazip_install\\lib\\libquazip1-qt5.a");
}
