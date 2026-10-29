// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

var mingw = "C:\\msys64\\ucrt64";
var qt_version = 6;

function setCommonFlags() {
    project.setVariable("MODULES", ["qt", "msi"]);

    var static_ = project.getConfig().indexOf("static") >= 0;

    var bin_dir = mingw + "\\bin";

    project.vars.LIBRARY_PATH = [
        mingw + "\\qt" + qt_version + "-static\\lib",
        mingw + "\\qt" + qt_version + "-static\\share\\qt6\\plugins\\imageformats",
        mingw + "\\qt" + qt_version + "-static\\share\\qt6\\plugins\\platforms",
        mingw + "\\qt" + qt_version + "-static\\share\\qt6\\plugins\\styles"
    ];

    project.setVariable("GCC_BIN_DIR", bin_dir);
    project.setVariable("AR", bin_dir + "\\ar.exe");
    project.setVariable("CC", bin_dir + "\\gcc.exe");
    project.setVariable("CXX", bin_dir + "\\g++.exe");
    project.setVariable("DEBUGGER", bin_dir + "\\gdb.exe");
    project.setVariable("NTLDD", bin_dir + "\\ntldd.exe");
    project.setVariable("PKG_CONFIG", bin_dir + "\\pkg-config.exe");
    project.setVariable("WINDRES", bin_dir + "\\windres.exe");

    project.setVariable("COMPANY", "Npackd");
    project.setVariable("WIX_LIGHT", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\light.exe");
    project.setVariable("WIX_CANDLE", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\candle.exe");

    var qt = mingw + "\\qt" + qt_version + "-static";
    project.setVariable("QMAKE", qt + "\\bin\\qmake.exe");
    project.setVariable("QT_MOC", qt + "\\share\\qt" + qt_version + "\\bin\\moc.exe");
    project.setVariable("QT_RCC", qt + "\\share\\qt" + qt_version + "\\bin\\rcc.exe");
    project.setVariable("QT_LRELEASE", qt + "\\bin\\lrelease.exe");
    project.setVariable("QT_LUPDATE", qt + "\\bin\\lupdate.exe");
    project.setVariable("QT_UIC", qt + "\\share\\qt" + qt_version + "\\bin\\uic.exe");

    var version = system.readTextFile(project.getDirectory() + "\\..\\appveyor.yml");
    version = version.split('\n')[0];
    version = version.split(':')[1];
    version = version.split('{')[0];
    version = version + "0";
    version = version.trim();
    console.log(version);
    project.setVariable("VERSION", version);

    /** @type Array<String> */
    var defines;
    
    /**  @ts-ignore */
    defines = project.getVariable("DEFINES");

    if (defines === null)
        defines = [];
    
    defines.push("UNICODE");
    defines.push("_UNICODE");
    defines.push("_WIN32_WINNT=0x0601");
    defines.push("NOMINMAX");
    defines.push("NDEBUG");
    defines.push("NPACKD_ADMIN=1");
    defines.push("NPACKD_VERSION=\"" + version + "\"");

    if (static_) {
        defines.push("QUAZIP_STATIC=1");
        defines.push("NPACKD_STATIC");
    }
    project.setVariable("DEFINES", defines);

    /** @type Array<String> */
    var ldflags;

    /**  @ts-ignore */
    ldflags = project.getVariable("LDFLAGS") || [];
    // "-Wl,-Map," + project.getName() +".map", 
    ldflags.push("-Wl,--subsystem," + project.getVariable("SUBSYSTEM") + ":6.1");
    if (static_) {
        ldflags = ldflags.concat(["-static", "-static-libstdc++", "-static-libgcc",
            "-L" + qt + "\\lib"]);
    }
    project.setVariable("LDFLAGS", ldflags);

    if (static_)
        project.setVariable("PKG_CONFIG_PATH", [
            qt + "\\lib\\pkgconfig",
            mingw + "/lib/pkgconfig",
            mingw + "/share/pkgconfig"]);

    /** @type Array<String> */
    var libs_cflags;

    /**  @ts-ignore */
    libs_cflags = project.getVariable("LIBS_CFLAGS") || [];
    if (static_) {
        libs_cflags.push("-I" + project.getDirectory() + "\\..\\quazip\\quazip\\quazip");
    }
    project.setVariable("LIBS_CFLAGS", libs_cflags);

    /** @type Array<String> */
    var cflags;

    /** @ts-ignore */
    cflags = project.getVariable("CFLAGS") || [];
    cflags.push("-g", "-Os", "-Wall", "-Wwrite-strings",
        "-Wextra", "-Wno-unused-parameter", "-Wno-cast-function-type",
        "-Wduplicated-cond", "-Wduplicated-branches", "-Wlogical-op", "-Wno-error=cast-qual",
        "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++17");

    if (project.getConfig().indexOf("debug") >= 0) {
        // -O0 outputs no warnings about uninitialized variables
        // GDB stops at wrong lines sometimes with -O2
        cflags.push("-O1");

        cflags.push("-fno-omit-frame-pointer");
    } else {
        cflags.push("-Os");
    }
    project.setVariable("CFLAGS", cflags);

    project.findPrlLibrary("Qt" + qt_version + "Core", { static: static_ });
    project.addLibraryInclude("Qt" + qt_version + "Core", qt + "\\include\\qt" + qt_version);
    project.addLibraryInclude("Qt" + qt_version + "Core", qt + "\\include\\qt" + qt_version + "\\QtCore");

    project.addLibraryInclude("Qt" + qt_version + "Xml", qt + "\\include\\qt" + qt_version + "\\QtXml");
    project.addLibraryInclude("Qt" + qt_version + "Sql", qt + "\\include\\qt" + qt_version + "\\QtSql");
    project.addLibraryInclude("Qt" + qt_version + "Gui", qt + "\\include\\qt" + qt_version + "\\QtGui");
    project.addLibraryInclude("Qt" + qt_version + "Widgets", qt + "\\include\\qt" + qt_version + "\\QtWidgets");

    if (static_) {
        var qt_plugins = qt + "\\share\\qt" + qt_version + "\\plugins\\";

        project.findPkgConfigLibrary("freetype", { package: "freetype2", static: static_ });

        project.findPrlLibrary("qgif", { static: static_ });
        project.findPrlLibrary("qicns", { static: static_ });
        project.findPrlLibrary("qico", { static: static_ });
        project.findPrlLibrary("qjpeg", { static: static_ });
        project.findPrlLibrary("qsvg", { static: static_ });
        project.findPrlLibrary("qtga", { static: static_ });
        project.findPrlLibrary("qtiff", { static: static_ });
        project.findPrlLibrary("qwbmp", { static: static_ });
        project.findPrlLibrary("qwebp", { static: static_ });
        project.findPrlLibrary("qwindows", { static: static_ });
        project.findPrlLibrary("qmodernwindowsstyle", { static: static_ });
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");

        project.addLibraryDependency("qwindows", "Wtsapi32");
        project.addLibraryDependency("qsqlite", "Qt" + qt_version + "Sql");

        // this is kinda strange. Qt should have its own libz and QuaZIP should depend on it,
        // but somehow the linker finds missing references to libz from Qt6Core.
        project.addLibraryDependency("Qt" + qt_version + "Core", "z");
    } else {
        project.findPkgConfigLibrary("quazip1-qt" + qt_version + "", { static: static_ });

        var qt_plugins = qt + "\\share\\qt" + qt_version + "\\plugins\\";
        project.setLibraryPath("qsqlite", qt_plugins + "sqldrivers\\libqsqlite.a");
    }
}
