// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl_ftests");
    project.setVariable("TYPE", "program");
    project.setVariable("DEPENDENCIES", ["npackd", "Qt5Test"]);
    project.setVariable("QT_MOC_FILES", ["app.h"]);

    system.include("..\\tb_common.js");
}
