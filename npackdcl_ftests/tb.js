// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl_ftests");
    project.setVariable("TYPE", "program");

    var static = project.getConfig().indexOf("static") >= 0;

    project.setVariable("DEPENDENCIES", ["npackd"]);

    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    project.findPkgConfigLibrary("Qt5Sql", { static: static });
    project.findPkgConfigLibrary("Qt5Xml", { static: static });
    project.findPkgConfigLibrary("Qt5Core", { static: static });
    project.findPkgConfigLibrary("Qt5Test", { static: static });

    if (!static)
        project.findPkgConfigLibrary("quazip1-qt5");
}
