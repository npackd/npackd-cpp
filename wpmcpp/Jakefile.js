var cp = require("child_process");
var fs = require('fs')

function path(package, version) {
	return cp.execSync('"' + process.env.NPACKD_CL + 
			'\\npackdcl.exe" "path" "--package=' + package + 
			'" "--versions=' + version + '"').toString().trim();
}

var MINGW = path("mingw-w64-i686-sjlj-posix", "[4.9.2, 4.9.2]");
var QUAZIP = path("quazip-dev-i686-w64-static", "[0.7.1, 0.7.1]");
var QT = "C:\\NpackdSymlinks\\com.nokia.QtDev-i686-w64-Npackd-Release-5.5";
var BITS = "32";
var PACKAGE = "com.googlecode.windows-package-manager.Npackd";
var CONFIG = "release";
var WHERE = "build\\32\\release";
var AI = path("com.advancedinstaller.AdvancedInstallerFreeware", "[10, 20)");
var SEVENZIP = path("org.7-zip.SevenZIP", "[9, 20)");
var MINGWUTILS = path("org.mingw.MinGWUtilities", "[0.3, 0.3]");
var PUTTY = path("uk.org.greenend.chiark.sgtatham.Putty", "[0.62, 2)");
var VERSION = fs.readFileSync("version.txt").toString();

desc('This is the default task');
task('default', function (params) {
	console.log("Default...");
});

desc('Print the found variables');
task('printvars', function (params) {
	console.log("MINGW:" + MINGW);
	console.log("QUAZIP:" + QUAZIP);
	console.log("QT:" + QT);
	console.log("BITS:" + BITS);
	console.log("PACKAGE:" + PACKAGE);
	console.log("CONFIG:" + CONFIG);
	console.log("WHERE:" + WHERE);
	console.log("AI:" + AI);
	console.log("SEVENZIP:" + SEVENZIP);
	console.log("MINGWUTILS:" + MINGWUTILS);
	console.log("PUTTY:" + PUTTY);
	console.log("VERSION:" + VERSION);
});

desc("Run qmake");
task("qmake", ["printvars"], function(params) {
	console.log("qmake...");
    var c = "set path=" + MINGW + "\\bin&&set quazip_path=" + QUAZIP + 
            "&& cd " + WHERE + "\\.. && \"" + QT + "\\qtbase\\bin\\qmake.exe\" ..\\..\\src\\wpmcpp.pro -r -spec win32-g++ CONFIG+=release32";
	cp.execSync(c, { stdio: 'inherit' });
});

desc('Delete temporary files and directories');
task('clean', ["printvars"], function (params) {
	console.log("Cleaning...");
    var c = "rmdir /s /q \"" + WHERE + "\"";
	cp.execSync(c, { stdio: 'inherit' });
});

desc('Prepare');
task('prep', ["compile"], function (params) {
	console.log("Preparing...");
    var c = "rmdir /s /q \"" + WHERE + "\"";
	cp.execSync(c, { stdio: 'inherit' });
    /*
	cd $(WHERE) && copy ..\wpmcpp_release.map Npackd$(BITS)-$(VERSION).map
	set path=$(MINGW)\bin&& "$(QT)\qtbase\bin\lrelease.exe" ..\..\..\src\wpmcpp.pro
	copy LICENSE.txt $(WHERE)\zip
	copy CrystalIcons_LICENSE.txt $(WHERE)\zip
	copy $(WHERE)\wpmcpp.exe $(WHERE)\zip\npackdg.exe
	"$(MINGW)\bin\strip.exe" $(WHERE)\zip\npackdg.exe
ifeq (32,$(BITS))
	copy "$(MINGWUTILS)\bin\exchndl.dll" $(WHERE)\zip
endif
 */
});

desc('Create the .zip file');
task('zip', ["prep"], function (params) {
	console.log("Zipping...");
    var c = "cd $(WHERE)\\zip && \"$(SEVENZIP)\\7z\" a ..\\Npackd$(BITS)-$(VERSION).zip *";
	cp.execSync(c, { stdio: 'inherit' });
});

desc('Create the .msi file');
task('msi', ["prep"], function (params) {
	console.log("Creating the .msi file...");
    var c = "cd $(WHERE)\\zip && \"$(SEVENZIP)\\7z\" a ..\\Npackd$(BITS)-$(VERSION).zip *";
	cp.execSync(c, { stdio: 'inherit' });
    /*
	"$(AI)\bin\x86\AdvancedInstaller.com" /edit src\wpmcpp$(BITS).aip /SetVersion $(VERSION)
	"$(AI)\bin\x86\AdvancedInstaller.com" /build src\wpmcpp$(BITS).aip
    */
});

desc('Compile the program');
task('compile', ["printvars"], function (params) {
	console.log("Compiling...");
    var c = "set path=" + MINGW + "\\bin&&set quazip_path=" + QUAZIP + 
            "&& cd " + WHERE + "\\.. && \"" + MINGW + "\\bin\\mingw32-make.exe\"";
	cp.execSync(c, { stdio: 'inherit' });
});

