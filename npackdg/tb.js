// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

// TODO: src/app.rc.in
// TODO: src/app.ico
// TODO: npackdcl.rc
// TODO: qsqlite
// TODO: qtpcre2 zstd z
// TODO: ../CrystalIcons_LICENSE.txt ../LICENSE.txt 

project.setName("npackdg");
project.setType("program");
project.setSubsystem("windows");
// TODO: Qt5FontDatabaseSupport Qt5::QSvgIconPlugin Qt5::QSvgPlugin
project.setDependencies(["npackd", "quazip1-qt5", "Qt5WinExtras", "Qt5Gui", "Qt5Svg",
    "Qt5Sql", "Qt5Xml", "Qt5Core",
    "imm32", "glu32", "mpr", "wtsapi32", "opengl32", "UxTheme", "Dwmapi",
    "oleaut32", "userenv", "winmm", "ole32",
    "uuid", "wininet", "psapi", "version", "shlwapi", "msi", "netapi32", "Ws2_32", "taskschd"]);

project.setQtMocFiles(["asyncdownloader.h", "exportrepositoryframe.h", "fileloader.h",
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