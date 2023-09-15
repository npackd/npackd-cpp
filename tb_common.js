// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

function setCommonFlags() {
    var version = system.readTextFile(project.getDirectory() + "\\..\\appveyor.yml");
    version = version.split('\n')[0];
    version = version.split(':')[1];
    version = version.split('{')[0];
    version = version + "0";
    version = version.trim();
    console.log(version);
    project.setVersion(version);

    var defines = project.getDefines();
    defines.UNICODE = "";
    defines._UNICODE = "";
    defines.NOMINMAX = "";
    defines.DNDEBUG = "";
    defines.NPACKD_ADMIN = "1";
    defines._WIN32_WINNT = "0x0601";
    defines.NPACKD_VERSION = "\"" + version + "\"";
    if (project.getConfig() === "static") {
        defines.QUAZIP_STATIC = "1";
    }
    project.setDefines(defines);

    // "-Wl,-Map," + project.getName() +".map", 
    var ldflags = ["-Wl,--subsystem," + project.getSubsystem() + ":6.1"];
    if (project.getConfig() === "static") {
        project.setPkgConfigDirs([
            "C:\\msys64/mingw64/lib/pkgconfig",
            "C:\\msys64/mingw64/share/pkgconfig",
            "C:\\msys64\\mingw64\\qt5-static\\lib\\pkgconfig"]);
        ldflags = ldflags.concat(["-static", "-static-libstdc++", "-static-libgcc",
            "-LC:\\msys64\\mingw64\\qt5-static\\lib", "-lzstd", "-lharfbuzz", "-lusp10", "-lgdi32", "-lrpcrt4",
            "-lgraphite2", "-lpng", "-lz", "-lpcre2-16"]);
    }
    project.setLinkerFlags(ldflags);

    project.setCFlags(["-g", "-Os", "-Wall", "-Wwrite-strings",
        "-Wextra", "-Wno-unused-parameter", "-Wno-cast-function-type",
        "-Wduplicated-cond", "-Wduplicated-branches", "-Wlogical-op", "-Wno-error=cast-qual",
        "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++11", "-IC:\\msys64\\mingw64\\include\\QuaZip-Qt5-1.4\\quazip"]);
}

setCommonFlags();
