const YAML = require('yaml');
var cp = require("child_process");
var fs = require('fs')
var mustache = require('mustache')
const path = require('path');

function shouldUpdate(a, b) {
	const sa = fs.statSync(a);
	var sb;
	try {
		sb = fs.statSync(b);
	} catch (e) {
		// the file does not exist
		return true;
	}
	return sa.mtime > sb.mtime;
}

function nclPath(package, version) {
	return cp.execSync('"' + process.env.NPACKD_CL + 
			'\\npackdcl.exe" "path" "--package=' + package + 
			'" "--versions=' + version + '"').toString().trim();
}

var MINGW = "C:/msys64/mingw64";
var QUAZIP = nclPath("quazip-dev-i686-w64-static", "[0.7.1, 0.7.1]");
var QT = "C:/msys64/mingw64/qt5-static";
var BITS = "64";
var PACKAGE = "com.googlecode.windows-package-manager.Npackd64";
var CONFIG = "release";
var WHERE = process.cwd();
var AI = nclPath("com.advancedinstaller.AdvancedInstallerFreeware", "[10, 20)");
var SEVENZIP = nclPath("org.7-zip.SevenZIP", "[9, 20)");
var MINGWUTILS = nclPath("org.mingw.MinGWUtilities", "[0.3, 0.3]");
var PUTTY = nclPath("uk.org.greenend.chiark.sgtatham.Putty", "[0.62, 2)");

const file = fs.readFileSync(__dirname + '/../appveyor.yml', 'utf8')
const appveyor = YAML.parse(file);

var VERSION = appveyor.version;
if (VERSION.endsWith("{build}"))
	VERSION = VERSION.substring(0, VERSION.length - 7);

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
task('prepare', [], function (params) {
	if (!fs.existsSync("src")){
		fs.mkdirSync("src");
	}
	if (!fs.existsSync("obj")){
		fs.mkdirSync("obj");
	}
	if (!fs.existsSync("dist")){
		fs.mkdirSync("dist");
	}

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
task('compile', ["prepare", 'moc', 'uic', 'compile-qt-resources'], function (params) {
	console.log("Compiling...");

	var files = fs.readdirSync(__dirname + "/src/");

	// defines
	var CFLAGS = "-DNPACKD_ADMIN=1 -DNPACKD_VERSION=\\\"1.27.0.0\\\" -DQT_CORE_LIB -DQT_NO_DEBUG -DQT_SQL_LIB -DQT_XML_LIB -DQUAZIP_STATIC=1 -DUNICODE -D_UNICODE -D_WIN32_WINNT=0x0600 -DNDEBUG -DQT_LINK_STATIC";

	// includes
	CFLAGS += " -IC:/builds/quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static/include";
	CFLAGS += " -isystem " + QT + "/include";
	CFLAGS += " -isystem " + QT + "/include/QtSql";
	CFLAGS += " -isystem " + QT + "/include/QtCore";
	CFLAGS += " -isystem " + QT + "/share/qt5/mkspecs/win32-g++";
	CFLAGS += " -isystem " + QT + "/include/QtXml";
	CFLAGS += " -isystem " + QT + "/include/QtWidgets";
	CFLAGS += " -isystem " + QT + "/include/QtGui";
	CFLAGS += " -isystem " + QT + "/include/QtWinExtras";
	CFLAGS += " -Isrc";
	CFLAGS += " -Ibuild/gensrc";

	CFLAGS += " -static -static-libstdc++ -static-libgcc -g -Os -Wall -Winit-self -Wwrite-strings -Wextra -Wno-long-long -Wno-overloaded-virtual -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unknown-pragmas -Wno-cast-function-type -Wno-unused-but-set-parameter -Wno-error=cast-qual -Wno-unused-local-typedefs -Wno-unused-variable -std=gnu++11";

    files.forEach(function(el, i) {
		var parsed = path.parse(el);
		var ext = parsed.ext.toLowerCase();

		if (ext === ".cpp" || ext === ".c") {
			var from = __dirname + "/src/" + el;
			var to = "obj/" + parsed.name + ".obj";
			if (shouldUpdate(from, to)) {
				var c = MINGW + "/bin/c++ " + CFLAGS + " -c " + from + " -o " + to;
				console.log(c);
				cp.execSync(c, {stdio: 'inherit'});
			}
		}
	});

	files = fs.readdirSync("src");
    files.forEach(function(el, i) {
		var parsed = path.parse(el);
		var ext = parsed.ext.toLowerCase();

		if (ext === ".cpp") {
			var from = "src/" + el;
			var to = "obj/" + parsed.name + ".obj";
			if (shouldUpdate(from, to)) {
				var c = MINGW + "/bin/c++ " + CFLAGS + " -c " + from + " -o " + to;
				console.log(c);
				cp.execSync(c, {stdio: 'inherit'});
			}
		}
	});
});

desc('Link the program');
task('link', ["prepare", 'compile', 'compile-resources'], function (params) {
	var files = fs.readdirSync("obj/");
	
	var objects = [];
	
    files.forEach(function(el, i) {
		var parsed = path.parse(el);
		var ext = parsed.ext.toLowerCase();

		if (ext === ".obj") {
			objects.push("obj/" + el);
		}
	});

	var LDFLAGS = "-static -static-libstdc++ -static-libgcc -g -Os";
	LDFLAGS += " -Wl,--subsystem,windows:6.1 -Wl,-Map,dist/npackdg.map -mwindows -Wl,--major-image-version,0,--minor-image-version,0 -Wl,-O2 -Wl,-s";
	LDFLAGS += " -L" + QT + "/share/qt5/plugins/platforms";
	LDFLAGS += " -L" + QT + "/share/qt5/plugins/imageformats";
	LDFLAGS += " -L" + QT + "/share/qt5/plugins/sqldrivers";
	LDFLAGS += " -L" + QT + "/share/qt5/plugins/styles";
	LDFLAGS += " -L" + QT + "/lib/";
	LDFLAGS += " -LC:/builds/quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static/lib";
	LDFLAGS += " -lquazip -lz -lqwindowsvistastyle -lQt5widgets -lqwindows -lQt5VulkanSupport -lQt5Gui -lQt5WinExtras -lQt5FontDatabaseSupport -lqsqlite -lQt5sql -lQt5xml -lQt5Core";
	LDFLAGS += " -lqicns -lqico -lqjpeg -lqgif -lqtga -lqtiff -lqwbmp -lqwebp";
	LDFLAGS += " -lmingwex -lQt5ThemeSupport -lQt5EventDispatcherSupport -lQt5FontDatabaseSupport -lQt5PlatformCompositorSupport -lQt5WindowsUIAutomationSupport -lqdirect2d";
	LDFLAGS += " -ljasper -licuin -licuuc -licudt -licutu -lqtpcre2 -lqtharfbuzz -lqtfreetype -lqtlibpng -ljpeg -lzstd -lz";
	LDFLAGS += " -limm32 -lwinmm -lglu32 -lmpr -luserenv -lwtsapi32 -lopengl32 -lole32 -luuid -lwininet -lpsapi -lversion -lshlwapi -lmsi -lnetapi32 -lWs2_32 -lUxTheme -lDwmapi -ltaskschd -loleaut32";

	var c = MINGW + "/bin/c++ -o dist/npackdg.exe " + objects.join(" ") + " " + LDFLAGS;
	cp.execSync(c, {stdio: 'inherit'});
});

desc('Updates the translations from the source code');
task('update-translations', [], function (params) {
	console.log("Updating translations...");

    ["de", "es", "fr", "ru"].forEach(function(el, i) {
		var c = QT + "/bin/lupdate src -locations none -ts src/npackdg_" + el + ".ts";
		cp.execSync(c, { stdio: 'inherit', cwd: __dirname });
	});
});

desc('Run MOC');
task('moc', ["prepare"], function (params) {
	var QTOBJECTS = "visiblejobs.h asyncdownloader.h clprogress.h downloader.h downloadsizefinder.h exportrepositoryframe.h fileloader.h";
	QTOBJECTS += " installedpackages.h job.h licenseform.h mainframe.h mainwindow.h messageframe.h packageframe.h packageversionform.h progresstree2.h";
	QTOBJECTS += " settingsframe.h urlinfo.h visiblejobs.h";

    QTOBJECTS.split(" ").forEach(function(el, i) {
		var from = __dirname + "/src/" + el;
		var to = "src/" + el + "_moc.cpp";
		if (shouldUpdate(from, to)) {
			var c = QT + "/bin/moc -o " + to + " " + from;
			cp.execSync(c, { stdio: 'inherit' });
		}
	});
});

desc('Run UIC');
task('uic', ["prepare"], function (params) {
	var files = fs.readdirSync(__dirname + "/src/");

    files.forEach(function(el, i) {
		if (el.toLowerCase().endsWith(".ui")) {
			var from = __dirname + "/src/" + el;
			var to = "src/ui_" + el.substring(0, el.length - 3) + ".h";
			if (shouldUpdate(from, to)) {
				var c = QT + "/bin/uic -o " + to + " " + from;
				cp.execSync(c, { stdio: 'inherit' });
			}
		}
	});
});

desc('Compile text translations');
task('compile-translations', ["prepare"], function (params) {
	var files = fs.readdirSync(__dirname + "/src/");

    files.forEach(function(el, i) {
		var parsed = path.parse(el);
		var ext = parsed.ext.toLowerCase();
		
		if (ext === ".ts") {
			var from = __dirname + "/src/" + el;
			var to = "src/" + parsed.name + ".qm";
			if (shouldUpdate(from, to)) {
				var c = QT + "/bin/lrelease " + from + " -qm " + to;
				cp.execSync(c, { stdio: 'inherit' });
			}
		}
	});
});

desc('Compile Qt resources');
task('compile-qt-resources', ["prepare", 'compile-translations'], function (params) {
	var from = __dirname + "/src/npackdg.qrc";
	var to = "src/npackdg.cpp";
	var c = QT + "/bin/rcc " + from + " -o " + to;
	cp.execSync(c, { stdio: 'inherit' });
});

desc('Compile program resources');
task('compile-resources', ["prepare", "template-app-rc"], function (params) {
	var from = __dirname + '/src/npackdg.manifest';
	var toManifest = "src/npackdg.manifest";
	if (shouldUpdate(from, toManifest)) {
		fs.copyFileSync(from, toManifest);
	}

	var from = __dirname + '/src/app.ico';
	var toIcon = "src/app.ico";
	if (shouldUpdate(from, toIcon)) {
		fs.copyFileSync(from, toIcon);
	}

	var from = "src/app.rc";
	var to = "obj/npackdg.obj";
	if (shouldUpdate(from, to)) {
		var c = "windres --use-temp-file --input " + from + " --output " + to;
		cp.execSync(c, { stdio: 'inherit' });
	}
});

desc('Replace variables in app.rc.in');
task('template-app-rc', ["prepare"], function (params) {
    var from = __dirname + '/src/app.rc.in';
	var to = "src/app.rc";
	
	if (shouldUpdate(from, to)) {
		const tmpl = fs.readFileSync(from, 'utf8');
		var view = {
		  a: "1",
		  b: "27",
		  c: "0",
		  d: "0",
		};
		var output = mustache.render(tmpl, view);
		fs.writeFileSync(to, output, "utf8");	
	}
});

