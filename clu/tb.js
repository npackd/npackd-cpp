// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>
/// <reference path="../tb_common.js"/>

system.include("..\\tb_common.js");

function configure() {
    project.setVariable("NAME", "clu");
    project.setVariable("TYPE", "program");

    var static_ = project.getConfig().indexOf("static") >= 0;

    var deps = ["npackd", "Qt" + qt_version + "Core"];
    if (static_) {
        deps.push("quazip");
    } else {
        deps.push("quazip1-qt" + qt_version);
    }
    project.setVariable("DEPENDENCIES", deps);

    setCommonFlags();
}
