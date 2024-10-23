// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl_tests");
    project.setVariable("TYPE", "program");
    project.setVariable("DEPENDENCIES", ["npackd", "Qt5Test" ]);
    
    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    var static = project.getConfig().indexOf("static") >= 0;
    project.findPkgConfigLibrary("Qt5Sql", { static: static });
    project.findPkgConfigLibrary("Qt5Xml", { static: static });
    project.findPkgConfigLibrary("Qt5Core", { static: static });
    project.findPkgConfigLibrary("Qt5Test", { static: static });

    if (!static)
        project.findPkgConfigLibrary("quazip1-qt5");
}
