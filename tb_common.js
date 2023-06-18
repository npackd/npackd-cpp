// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

var version = system.readTextFile(project.getDirectory() + "\\..\\appveyor.yml");
version = version.split('\n')[0];
version = version.split(':')[1];
version = version.split('{')[0];
version = version + "0";
version = version.trim();
console.log(version);
project.setVersion(version);
project.setDefines({
    NPACKD_VERSION: "\"" + version + "\"",
    NOMINMAX: "",
    NPACKD_ADMIN: "1",
    // TODO QUAZIP_STATIC: "1",
    UNICODE: "",
    _UNICODE: "",
    _WIN32_WINNT: "0x0601",
    DNDEBUG: ""
});

var ldflags = ["-Wl,-Map," + project.getName() +
    ".map", "-Wl,--subsystem," + project.getSubsystem() + ":6.1"];
if (false) { // TODO parameter for static build
    project.setPkgConfigDirs(["C:\\msys64\\mingw64\\qt5-static\\lib\\pkgconfig", "C:\\msys64/mingw64/lib/pkgconfig", "C:\\msys64/mingw64/share/pkgconfig"]);
    ldflags = ldflags.concat(["-static", "-static-libstdc++", "-static-libgcc",
        "-LC:\\msys64\\mingw64\\qt5-static\\lib", "-LC:\\builds\\quazip\\quazip", "-lzstd", "-lharfbuzz", "-lusp10", "-lgdi32", "-lrpcrt4",
        "-lgraphite2", "-lpng", "-lz", "-lpcre2-16"]);
}
project.setLinkerFlags(ldflags);


project.setCFlags(["-g", "-Os", "-Wall", "-Wwrite-strings",
    "-Wextra", "-Wno-unused-parameter", "-Wno-cast-function-type",
    "-Wduplicated-cond", "-Wduplicated-branches", "-Wlogical-op", "-Wno-error=cast-qual",
    "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++11"]);
