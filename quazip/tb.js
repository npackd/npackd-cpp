// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

function configure() {

}

function build() {
    console.log("Building QuaZIP................");
    child_process.execFileSync("C:\\msys64\\mingw64\\qt5-static\\bin\\qmake.exe", ["--qt-version"]);
}
