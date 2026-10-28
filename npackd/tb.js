// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>
/// <reference path="../tb_common.js"/>

system.include("..\\tb_common.js");

function configure() {
    project.setVariable("NAME", "npackd");
    project.setVariable("TYPE", "library");

    var static_ = project.getConfig().indexOf("static") >= 0;

    var deps = ["Qt" + qt_version + "Sql", "Qt" + qt_version + "Xml", "Qt" + qt_version + "Core", "userenv", "winmm", "ole32",
        "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd", "OleAut32"];
    if (static_) {
        deps.push("quazip");
        deps.push("qsqlite");
   } else {
        deps.push("quazip1-qt" + qt_version);
    }
    project.setVariable("DEPENDENCIES", deps);
    project.vars.QT_MOC_FILES = ["clprogress.h", "downloader.h", "installedpackages.h", "job.h"];

    setCommonFlags();
}

