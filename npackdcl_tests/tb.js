// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    project.setVariable("NAME", "npackdcl_tests");
    project.setVariable("TYPE", "program");
    project.setVariable("DEPENDENCIES", ["npackd", "Qt5Test" ]);
    
    var static_ = project.getConfig().indexOf("static") >= 0;

    project.setVariable("QT_MOC_FILES", ["app.h"]);
    system.include("..\\tb_common.js");

    var static_ = project.getConfig().indexOf("static") >= 0;
    project.findPkgConfigLibrary("Qt5Sql", { static: static_ });
    project.findPkgConfigLibrary("Qt5Xml", { static: static_ });
    project.findPkgConfigLibrary("Qt5Core", { static: static_ });
    project.findPkgConfigLibrary("Qt5Test", { static: static_ });

    if (!static_) {
        project.findPkgConfigLibrary("quazip1-qt5");
    } else {
        var qt = "C:\\msys64\\mingw64\\qt5-static\\";
        var qt_plugins = qt + "share\\qt5\\plugins\\";
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");
    }
}
