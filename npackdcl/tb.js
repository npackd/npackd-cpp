// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

system.include("..\\tb_common.js");

function configure() {
    project.setVariable("NAME", "npackdcl");
    project.setVariable("TYPE", "program");
    project.setVariable("TITLE", "NpackdCL");
    project.setVariable("DESCRIPTION", "This is a Windows command line Npackd package manager client.");
    project.setVariable("MSI_UPGRADE_CODE", "B57EADCE-24E8-4D80-8611-25F58745FA25");
    project.setVariable("DEPENDENCIES", ["npackd"]);
    project.setVariable("QT_MOC_FILES", ["app.h"]);

    setCommonFlags();
}
