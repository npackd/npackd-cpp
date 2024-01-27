// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

// TODO: qsqlite
// TODO: qtpcre2 zstd z

project.setVariable("NAME", "npackdg");
project.setVariable("TYPE", "program");
project.setVariable("SUBSYSTEM", "windows");
// TODO: Qt5FontDatabaseSupport Qt5::QSvgIconPlugin Qt5::QSvgPlugin
project.setVariable("DEPENDENCIES", ["npackd", "quazip1-qt5", "Qt5WinExtras", "Qt5Gui", "Qt5Svg",
    "Qt5Sql", "Qt5Xml", "Qt5Core",
    "imm32", "glu32", "mpr", "wtsapi32", "opengl32", "UxTheme", "Dwmapi",
    "oleaut32", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);

project.setVariable("QT_MOC_FILES", ["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
    "licenseform.h", "mainframe.h", "mainwindow.h", "messageframe.h", "packageframe.h",
    "packageitemmodel.h", "packageversionform.h", "progresstree2.h", "repositoriesitemmodel.h",
    "visiblejobs.h", "settingsframe.h"]);

system.include("..\\tb_common.js");

/* TODO
src/npackdg_es.ts
    src/npackdg_ru.ts
    src/npackdg_fr.ts
    src/npackdg_de.ts
    */

project.findPkgConfigLibrary("Qt5Sql");
project.findPkgConfigLibrary("Qt5Xml");
project.findPkgConfigLibrary("Qt5Core");
project.findPkgConfigLibrary("Qt5WinExtras");
project.findPkgConfigLibrary("Qt5Gui");
project.findPkgConfigLibrary("Qt5Svg");
project.findPkgConfigLibrary("quazip1-qt5");
