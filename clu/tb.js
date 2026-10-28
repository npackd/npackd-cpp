// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

system.include("..\\tb_common.js");

function configure() {
    project.setVariable("NAME", "clu");
    project.setVariable("TYPE", "program");

    var static_ = project.getConfig().indexOf("static") >= 0;

    var deps = ["npackd", "Qt5Core"];
    if (static_) {
        deps.push("quazip");
    } else {
        deps.push("quazip1-qt5");
    }
    project.setVariable("DEPENDENCIES", deps);

    setCommonFlags();
}
