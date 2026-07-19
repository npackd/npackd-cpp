// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl");
    project.setVariable("TYPE", "program");
    project.setVariable("TITLE", "NpackdCL");
    project.setVariable("DESCRIPTION", "This is a Windows command line Npackd package manager client.");
    project.setVariable("MSI_UPGRADE_CODE", "B57EADCE-24E8-4D80-8611-25F58745FA25");

    var static_ = project.getConfig().indexOf("static") >= 0;

    project.setVariable("DEPENDENCIES", ["npackd"]);

    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    project.findPkgConfigLibrary("Qt5Sql", { static: static_ });
    project.findPkgConfigLibrary("Qt5Xml", { static: static_ });
    project.findPkgConfigLibrary("Qt5Core", { static: static_ });

    if (!static_) {
        project.findPkgConfigLibrary("quazip1-qt5", { static: static_ });
    } else {
        var qt = "C:\\msys64\\mingw64\\qt5-static\\";
        var qt_plugins = qt + "share\\qt5\\plugins\\";
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");
    }
}
