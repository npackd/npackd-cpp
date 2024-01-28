// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

var static = project.getConfig().indexOf("static") >= 0;

project.setVariable("NAME", "npackdg");
project.setVariable("TYPE", "program");
project.setVariable("SUBSYSTEM", "windows");

var deps = ["npackd", "quazip1-qt5", "Qt5WinExtras", "Qt5Gui", "Qt5Svg",
    "Qt5Sql", "Qt5Xml", "Qt5Core", "imm32", "glu32", "mpr", "wtsapi32", "opengl32", "UxTheme", "Dwmapi",
    "oleaut32", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"];
if (static) {
    deps.push("qwindows");
    deps.push("qwindowsvistastyle");
    deps.push("qminimal");
}
project.setVariable("DEPENDENCIES", deps);

project.setVariable("QT_MOC_FILES", ["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
    "licenseform.h", "mainframe.h", "mainwindow.h", "messageframe.h", "packageframe.h",
    "packageitemmodel.h", "packageversionform.h", "progresstree2.h", "repositoriesitemmodel.h",
    "visiblejobs.h", "settingsframe.h"]);

system.include("..\\tb_common.js");

project.findPkgConfigLibrary("Qt5Sql", { static: static });
project.findPkgConfigLibrary("Qt5Xml", { static: static });
project.findPkgConfigLibrary("Qt5Core", { static: static });
project.findPkgConfigLibrary("Qt5WinExtras", { static: static });
project.findPkgConfigLibrary("Qt5Gui", { static: static });
project.findPkgConfigLibrary("Qt5Svg", { static: static });
project.findPkgConfigLibrary("quazip1-qt5", { static: static });

if (static) {
    project.findPkgConfigLibrary("freetype", { package: "freetype2", static: static });
    
    var qt_plugins = "C:\\msys64\\mingw64\\qt5-static\\share\\qt5\\plugins\\";

    project.setLibraryPath("quazip1-qt5", "C:\\builds\\quazip_install\\lib\\libquazip1-qt5.a");
    project.setLibraryPath("qwindows", qt_plugins + "platforms\\libqwindows.a");
    project.setLibraryPath("qwindowsvistastyle", qt_plugins + "styles\\libqwindowsvistastyle.a");
    project.setLibraryPath("qminimal", qt_plugins + "platforms\\libqminimal.a");
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
    project.setLibraryPath("qtlibjpeg", "C:\\msys64\\mingw64\\qt5-static\\lib\\libqtlibjpeg.a");
    project.addLibraryDependency("Qt5FontDatabaseSupport", "freetype");
    project.addLibraryDependency("qwindows", "Qt5FontDatabaseSupport");
    project.addLibraryDependency("qwindows", "Qt5EventDispatcherSupport");
    project.addLibraryDependency("qwindows", "Wtsapi32");
    project.addLibraryDependency("qwindows", "Qt5ThemeSupport");
    project.addLibraryDependency("qwindows", "Qt5VulkanSupport");
    project.addLibraryDependency("qwindows", "Qt5WindowsUIAutomationSupport");
    project.addLibraryDependency("qjpeg", "qtlibjpeg");
}
