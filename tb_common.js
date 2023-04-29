// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>

var version = system.readTextFile(project.getDirectory() + "\\..\\appveyor.yml");
version = version.split('\n')[0];
version = version.split(':')[1];
version = version.split('{')[0];
version = version + "0";
version = version.trim();
console.log(version);
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

project.setCFlags(["-g", "-Os", "-Wl,--subsystem," + project.getSubsystem() +
    ":6.1", "-Wall", "-Wwrite-strings",
    "-Wextra", "-Wno-unused-parameter", "-Wno-cast-function-type",
    "-Wduplicated-cond", "-Wduplicated-branches", "-Wlogical-op", "-Wno-error=cast-qual",
    "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-Os", "-std=gnu++11"]);
