// @ts-check
/// <reference path="../../super/tb/install/builtins.js"/>
/// <reference path="../tb_common.js"/>

system.include("..\\tb_common.js");

var cmake = mingw + "\\bin\\cmake.exe";
var configDir = project.getDirectory() + "\\build\\" + project.getConfig();

function configure() {
    console.log("Configuring QuaZIP................");
    project.setVariable("NAME", "quazip");
    child_process.execFileSync(cmake, ["-GNinja", "-S", project.getDirectory() + "\\quazip",
        "-B", configDir,
        "-DCMAKE_MAKE_PROGRAM=" + mingw + "/bin/ninja.exe",
        "-DCMAKE_C_COMPILER=" + mingw + "/bin/gcc.exe",
        "-DCMAKE_CXX_COMPILER=" + mingw + "/bin/g++.exe",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DQUAZIP_QT_MAJOR_VERSION=" + qt_version,
        "-DQUAZIP_FETCH_LIBS=OFF",
        "-DQUAZIP_BZIP2=OFF",
        "-DBUILD_SHARED_LIBS:BOOL=OFF",
        "-DQUAZIP_ENABLE_QTEXTCODEC:BOOL=OFF",
        //"-DQUAZIP_USE_QT_ZLIB:BOOL=ON",
        //"-DZLIB_LIBRARY_RELEASE=" + mingw + "/lib/libz.a",
        "-DCMAKE_PREFIX_PATH=" + mingw + "/qt" + qt_version + "-static/lib/cmake"],
        {
            env: {
                PATH: mingw + "/bin;C:\\windows\\system32;C:\\windows", TEMP: "C:/Users2\\timle\\AppData\\Local\\Temp",
                // Please note that it should be C:/ and not C:\. CMake (or gcc) fails otherwise.
                TMP: "C:/Users\\timle\\AppData\\Local\\Temp",
                HOMEDRIVE: "C:"
            }
        });

    project.setVariable("DEPENDENCIES", ["z", "Qt" + qt_version + "Core"])
}

function build() {
    console.log("Building QuaZIP................");
    child_process.execFileSync(cmake, ["--build", configDir],
        {
            env: {
                PATH: mingw + "/bin;C:\\windows\\system32;C:\\windows", TEMP: "C:/Users2\\timle\\AppData\\Local\\Temp",
                // Please note that it should be C:/ and not C:\. CMake (or gcc) fails otherwise.
                TMP: "C:/Users\\timle\\AppData\\Local\\Temp",
                HOMEDRIVE: "C:"
            }
        });

    var src = configDir + "\\quazip\\libquazip1-qt" + qt_version + ".a";
    console.log("Copying " + src);
    fs.copyFileSync(src, configDir + "\\libquazip.a");
}
