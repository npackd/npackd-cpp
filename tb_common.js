// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

function setCommonFlags() {
    var static = project.getConfig().indexOf("static") >= 0;

    project.findBinaries("C:\\msys64\\mingw64\\bin");

    project.setVariable("COMPANY", "Npackd");
    project.setVariable("WIX_LIGHT", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\light.exe");
    project.setVariable("WIX_CANDLE", "C:\\Program Files (x86)\\WiX Toolset v3.11\\bin\\candle.exe");

    var version = system.readTextFile(project.getDirectory() + "\\..\\appveyor.yml");
    version = version.split('\n')[0];
    version = version.split(':')[1];
    version = version.split('{')[0];
    version = version + "0";
    version = version.trim();
    console.log(version);
    project.setVariable("VERSION", version);

    /** @type Array */
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

    if (static) {
        defines.push("QUAZIP_STATIC=1");
        defines.push("NPACKD_STATIC");
    }
    project.setVariable("DEFINES", defines);

    // "-Wl,-Map," + project.getName() +".map", 
    var ldflags = ["-Wl,--subsystem," + project.getVariable("SUBSYSTEM") + ":6.1"];
    if (static) {
        project.setVariable("PKG_CONFIG_PATH", [
            "C:\\msys64/mingw64/lib/pkgconfig",
            "C:\\msys64/mingw64/share/pkgconfig",
            "C:\\msys64\\mingw64\\qt5-static\\lib\\pkgconfig"]);
        ldflags = ldflags.concat(["-static", "-static-libstdc++", "-static-libgcc",
            "-LC:\\msys64\\mingw64\\qt5-static\\lib"]);
    }
    project.setVariable("LDFLAGS", ldflags);

    var cflags = ["-g", "-Os", "-Wall", "-Wwrite-strings",
        "-Wextra", "-Wno-unused-parameter", "-Wno-cast-function-type",
        "-Wduplicated-cond", "-Wduplicated-branches", "-Wlogical-op", "-Wno-error=cast-qual",
        "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++11", "-IC:\\msys64\\mingw64\\include\\QuaZip-Qt5-1.4\\quazip"];
    if (project.getConfig() === "debug") {
        // -O0 outputs no warnings about uninitialized variables
        // GDB stops at wrong lines sometimes with -O2
        cflags.push("-O1");

        cflags.push("-g");
        cflags.push("-fno-omit-frame-pointer");
    } else {
        cflags.push("-Os");
    }
    project.setVariable("CFLAGS", cflags);
}

setCommonFlags();
