// @ts-check
/// <reference path="../super/tb/install/builtins.js"/>
/// <reference path="tb_common.js"/>

function configure() {
    project.setVariable("NAME", "npackd-cpp");
    project.setVariable("TITLE", "Npackd C++");
    project.setVariable("DESCRIPTION", "All the C++ Npackd programs.");
    project.setVariable("TYPE", "generic");
}

/**
 * Re-build a sub-project.
 * 
 * @param {String} name project name
 */
function rebuildSubProject(name) {
    child_process.execFileSync("tb.exe", ["--configure", "-c", project.getConfig()], 
        {cwd: project.getDirectory() + "\\" + name});
    child_process.execFileSync("tb.exe", ["-c", project.getConfig(), "-t", "all"], 
        {cwd: project.getDirectory() + "\\" + name});
}

/**
 * Builds a sub-project.
 * 
 * @param {String} name project name
 * @param {String} target target
 */
function buildSubProjectTarget(name, target) {
    child_process.execFileSync("tb.exe", ["-c", project.getConfig(), "-t", target], 
        {cwd: project.getDirectory() + "\\" + name});
}

function build() {
    console.log("Building Npackd C++................");

    var version = system.readTextFile(project.getDirectory() + "\\appveyor.yml");
    version = version.split('\n')[0];
    version = version.split(':')[1];
    version = version.split('{')[0];
    version = version + "0";
    version = version.trim();
    console.log(version);

    var mingw = "C:\\msys64\\mingw64";

    var config = project.getConfig();
    var myDir = project.getDirectory();
    var d = myDir + "\\build\\" + config + "\\dist";
    fs.mkdirSync(d, { recursive: true });

    var name = "clu";
    var dist = myDir + "\\" + name + "\\build\\" + config + "\\dist";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);
    system.zip(dist, d + "\\CLU-debug-" + version + ".zip");
    child_process.execFileSync(mingw + "\\bin\\strip.exe", [dist + "\\clu.exe"]);
    system.zip(dist, d + "\\CLU-" + version + ".zip");

    var name = "npackd";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);

    var name = "npackdcl";
    dist = myDir + "\\" + name + "\\build\\" + config + "\\dist";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);
    system.zip(dist, 
        d + "\\NpackdCL-debug-" + version + ".zip");
    child_process.execFileSync(mingw + "\\bin\\strip.exe", [dist + "\\npackdcl.exe"]);
    system.zip(dist, d + "\\NpackdCL-" + version + ".zip");

    var name = "npackdcl_ftests";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);
    
    var name = "npackdcl_tests";
    dist = myDir + "\\" + name + "\\build\\" + config + "\\dist";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);
    //child_process.execFileSync(dist + "\\npackdcl_tests.exe");
    
    var name = "npackdg";
    dist = myDir + "\\" + name + "\\build\\" + config + "\\dist";
    console.log("We are building " + name + "................");
    rebuildSubProject(name);
    system.zip(dist, d + "\\Npackd-debug-" + version + ".zip");
    child_process.execFileSync(mingw + "\\bin\\strip.exe", [dist + "\\npackdg.exe"]);
    system.zip(dist, d + "\\Npackd-" + version + ".zip");
    buildSubProjectTarget(name, "msi");
    fs.mkdirSync(myDir + "\\" + name + "\\build\\" + config + "\\installer", { recursive: true });
    fs.copyFileSync(myDir + "\\" + name + "\\build\\" + config + "\\installer\\npackdg.msi", 
        d + "\\Npackd.msi");

    // TODO: exchndl.dll, mgwhelp.dll, dbghelp.dll, symsrv.dll, symsrv.yes, xxx.map
    // TODO: release on Github

    // TODO: Coverity
    /*
rem Coverity build is too slow

set path=%mingw%\bin;%ai%\bin\x86;%sevenzip%;C:\msys64\mingw64\bin
set CMAKE_PREFIX_PATH=%mingw%\%mingw_libs%
mingw32-make.exe clean

"..\..\cov-analysis\bin\cov-configure.exe"  --comptype gcc --compiler C:\PROGRA~2\MINGW-~1\bin\G__~1.EXE -- -std=gnu++11
"..\..\cov-analysis\bin\cov-build.exe" --dir ..\..\cov-int mingw32-make.exe install

rem type C:\projects\Npackd\cov-int\build-log.txt

7z a cov-int.zip cov-int
if %errorlevel% neq 0 exit /b %errorlevel%

"C:\Program Files (x86)\Gow\bin\curl" --form token=%covtoken% --form email=tim.lebedkov@gmail.com -k --form file=@cov-int.zip --form version="Version" --form description="Description" https://scan.coverity.com/builds?project=Npackd
if %errorlevel% neq 0 exit /b %errorlevel%
    */
}
