// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {
    // TODO: option(NPACKD_ADMIN "Force admin right on program" TRUE)

    var static_ = project.getConfig().indexOf("static") >= 0;

    project.setVariable("NAME", "npackdg");
    project.setVariable("TITLE", "Npackd");
    project.setVariable("DESCRIPTION", "This is a Windows graphical user interface for the client to the Npackd package manager.");
    project.setVariable("TYPE", "program");
    project.setVariable("SUBSYSTEM", "windows");
    project.setVariable("MSI_UPGRADE_CODE", "EABC4270-7D7F-47BF-B388-AC63606E8DAC");

    var deps = ["npackd", "Qt5Gui", "Qt5Svg"];
    if (static_) {
        deps.push("qwindows");
        deps.push("qwindowsvistastyle");
        deps.push("qsqlite");
        deps.push("qicns");
        deps.push("qico");
        deps.push("qjpeg");
        deps.push("qgif");
        deps.push("qtiff");
        deps.push("qwbmp");
        deps.push("qwebp");
        deps.push("qsvg");
        deps.push("qtlibjpeg");
        deps.push("qtga");
    }
    project.setVariable("DEPENDENCIES", deps);

    project.setVariable("QT_MOC_FILES", ["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
        "licenseform.h", "mainframe.h", "mainwindow.h", "messageframe.h", "packageframe.h",
        "packageitemmodel.h", "packageversionform.h", "progresstree2.h", "repositoriesitemmodel.h",
        "visiblejobs.h", "settingsframe.h"]);

    system.include("..\\tb_common.js");

    project.findPkgConfigLibrary("Qt5Sql", { static: static_ });
    project.findPkgConfigLibrary("Qt5Xml", { static: static_ });
    project.findPkgConfigLibrary("Qt5Core", { static: static_ });
    project.findPkgConfigLibrary("Qt5WinExtras", { static: static_ });
    project.findPkgConfigLibrary("Qt5Gui", { static: static_ });
    project.findPkgConfigLibrary("Qt5Svg", { static: static_ });
    project.findPkgConfigLibrary("Qt5Widgets", { static: static_ });

    if (!static_)
        project.findPkgConfigLibrary("quazip1-qt5");

    if (static_) {
        project.findPkgConfigLibrary("freetype", { package: "freetype2", static: static_ });

        var qt = "C:\\msys64\\mingw64\\qt5-static\\";
        var qt_plugins = qt + "share\\qt5\\plugins\\";

        project.setLibraryPath("qwindows", qt_plugins + "platforms\\libqwindows.a");
        project.setLibraryPath("qwindowsvistastyle", qt_plugins + "styles\\libqwindowsvistastyle.a");
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");
        project.setLibraryPath("qicns", qt_plugins + "imageformats\\libqicns.a");
        project.setLibraryPath("qico", qt_plugins + "imageformats\\libqico.a");
        project.setLibraryPath("qjpeg", qt_plugins + "imageformats\\libqjpeg.a");
        project.setLibraryPath("qgif", qt_plugins + "imageformats\\libqgif.a");
        project.setLibraryPath("qtga", qt_plugins + "imageformats\\libqtga.a");
        project.setLibraryPath("qtiff", qt_plugins + "imageformats\\libqtiff.a");
        project.setLibraryPath("qwbmp", qt_plugins + "imageformats\\libqwbmp.a");
        project.setLibraryPath("qwebp", qt_plugins + "imageformats\\libqwebp.a");
        project.setLibraryPath("qsvg", qt_plugins + "imageformats\\libqsvg.a");
        project.setLibraryPath("qtlibjpeg", qt + "lib\\libqtlibjpeg.a");

        project.addLibraryDependency("Qt5FontDatabaseSupport", "freetype");
        project.addLibraryDependency("Qt5FontDatabaseSupport", "Qt5Gui");

        project.addLibraryDependency("qwindows", "Qt5FontDatabaseSupport");
        project.addLibraryDependency("qwindows", "Qt5EventDispatcherSupport");
        project.addLibraryDependency("qwindows", "Wtsapi32");
        project.addLibraryDependency("qwindows", "Qt5ThemeSupport");
        project.addLibraryDependency("qwindows", "Qt5VulkanSupport");
        project.addLibraryDependency("qwindows", "Qt5WindowsUIAutomationSupport");
        project.addLibraryDependency("qjpeg", "qtlibjpeg");
    }
}
