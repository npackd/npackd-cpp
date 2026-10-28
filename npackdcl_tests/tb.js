// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

system.include("..\\tb_common.js");

function configure() {
    project.setVariable("NAME", "npackdcl_tests");
    project.setVariable("TYPE", "program");
    project.setVariable("DEPENDENCIES", ["npackd", "Qt5Test" ]);
    project.setVariable("QT_MOC_FILES", ["app.h"]);

    setCommonFlags();
}
