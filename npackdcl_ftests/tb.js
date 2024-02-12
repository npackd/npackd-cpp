// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "ftests");
    project.setVariable("TYPE", "program");

    var static = project.getConfig().indexOf("static") >= 0;

    var deps = ["npackd"];
    if (static) {
        deps.push("quazip");
    } else {
        deps.push("quazip1-qt5");
    }
    project.setVariable("DEPENDENCIES", deps);

    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    project.findPkgConfigLibrary("Qt5Sql");
    project.findPkgConfigLibrary("Qt5Xml");
    project.findPkgConfigLibrary("Qt5Core");
    project.findPkgConfigLibrary("Qt5Test");

    if (!static)
        project.findPkgConfigLibrary("quazip1-qt5");
}
