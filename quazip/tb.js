// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>

var cmake = "C:\\msys64\\mingw64\\bin\\cmake.exe";
var configDir = project.getDirectory() + "\\build\\" + project.getConfig();

function configure() {
    console.log("Configuring QuaZIP................");
    project.setVariable("NAME", "quazip");
    child_process.execFileSync(cmake, ["-GNinja", "-S", project.getDirectory() + "\\quazip",
        "-B", configDir,
        "-DCMAKE_MAKE_PROGRAM=C:/msys64/mingw64/bin/ninja.exe",
        "-DCMAKE_C_COMPILER=C:/msys64/mingw64/bin/gcc.exe",
        "-DCMAKE_CXX_COMPILER=C:/msys64/mingw64/bin/g++.exe",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DQUAZIP_QT_MAJOR_VERSION=5",
        "-DQUAZIP_FETCH_LIBS=OFF",
        "-DQUAZIP_BZIP2=OFF",
        "-DBUILD_SHARED_LIBS:BOOL=OFF",
        "-DZLIB_LIBRARY_RELEASE=C:/msys64/mingw64/lib/libz.a",
        "-DCMAKE_PREFIX_PATH=C:/msys64/mingw64/qt5-static/lib/cmake"],
        {
            env: {
                PATH: "C:/msys64/mingw64/bin;C:\\windows\\system32;C:\\windows", TEMP: "C:/Users2\\timle\\AppData\\Local\\Temp",
                // Please note that it should be C:/ and not C:\. CMake (or gcc) fails otherwise.
                TMP: "C:/Users\\timle\\AppData\\Local\\Temp",
                HOMEDRIVE: "C:"
            }
        });

    project.setVariable("DEPENDENCIES", ["z", "Qt5Core"])
}

function build() {
    console.log("Building QuaZIP................");
    child_process.execFileSync(cmake, ["--build", configDir],
        {
            env: {
                PATH: "C:/msys64/mingw64/bin;C:\\windows\\system32;C:\\windows", TEMP: "C:/Users2\\timle\\AppData\\Local\\Temp",
                // Please note that it should be C:/ and not C:\. CMake (or gcc) fails otherwise.
                TMP: "C:/Users\\timle\\AppData\\Local\\Temp",
                HOMEDRIVE: "C:"
            }
        });

    var src = configDir + "\\quazip\\libquazip1-qt5.a";
    console.log("Copying " + src);
    fs.copyFileSync(src, configDir + "\\libquazip.a");
}
