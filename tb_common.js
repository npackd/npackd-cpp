// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

function setCommonFlags() {
    project.setVariable("MODULES", ["qt", "msi"]);

    var static_ = project.getConfig().indexOf("static") >= 0;

    var mingw = "C:\\msys64\\mingw64";
    var qt = mingw + "\\qt5-static";

    project.findBinaries(mingw + "\\bin");

    project.setVariable("COMPANY", "Npackd");
    project.setVariable("WIX_LIGHT", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\light.exe");
    project.setVariable("WIX_CANDLE", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\candle.exe");

    project.setVariable("QT_LRELEASE", qt + "\\bin\\lrelease.exe");
    project.setVariable("QT_LUPDATE", qt + "\\bin\\lupdate.exe");
    project.setVariable("QT_UIC", qt + "\\bin\\uic.exe");

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
    defines.push("DNDEBUG");
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
            "C:\\msys64/mingw64/lib/pkgconfig",
            "C:\\msys64/mingw64/share/pkgconfig"]);

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
        "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++11");

    if (project.getConfig() === "debug") {
        // -O0 outputs no warnings about uninitialized variables
        // GDB stops at wrong lines sometimes with -O2
        cflags.push("-O1");

        cflags.push("-fno-omit-frame-pointer");
    } else {
        cflags.push("-Os");
    }
    project.setVariable("CFLAGS", cflags);
}

setCommonFlags();
