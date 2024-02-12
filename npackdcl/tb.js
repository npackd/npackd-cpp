// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl");
    project.setVariable("TYPE", "program");

    var static = project.getConfig().indexOf("static") >= 0;

    var deps = ["npackd"];
    // if (static) {
    //     deps.push("quazip");
    // } else {
    //     deps.push("quazip1-qt5");
    // }
    project.setVariable("DEPENDENCIES", deps);

    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    var static = project.getConfig().indexOf("static") >= 0;

    project.findPkgConfigLibrary("Qt5Sql", { static: static });
    project.findPkgConfigLibrary("Qt5Xml", { static: static });
    project.findPkgConfigLibrary("Qt5Core", { static: static });

    if (!static)
        project.findPkgConfigLibrary("quazip1-qt5", { static: static });
}
