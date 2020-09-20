
QT = "C:/msys64/mingw64/qt5-static";

task("echo")

    -- Set the run script
    on_run(function ()

        -- Import parameter option module
        import("core.base.option")

        -- Initialize color mode
        local modes = ""
        for _, mode in ipairs({"bright", "dim", "blink", "reverse"}) do
            if option.get(mode) then
                modes = modes .. " " .. mode
            end
        end

        -- Get parameter content and display information
        cprint("${%s%s}%s", option.get("color"), modes, table.concat(option.get("contents") or {}, " "))
    end)

    -- Set the command line options for the plugin. There are no parameter options here, just the plugin description.
    set_menu {
                -- Settings menu usage
                usage = "xmake echo [options]"

                -- Setup menu description
            ,   description = "Echo the given info!"

                -- Set menu options, if there are no options, you can set it to {}
            ,   options =
                {
                    -- Set k mode as key-only bool parameter
                    {'b', "bright", "k", nil, "Enable bright." }
                ,   {'d', "dim", "k", nil, "Enable dim." }
                ,   {'-', "blink", "k", nil, "Enable blink." }
                ,   {'r', "reverse", "k", nil, "Reverse color." }

                    -- When the menu is displayed, a blank line
                ,   {}

                    -- Set kv as the key-value parameter and set the default value: black
                ,   {'c', "color", "kv", "black", "Set the output color."
                                                     , " - red"
                                                     , " - blue"
                                                     , " - yellow"
                                                     , " - green"
                                                     , " - magenta"
                                                     , " - cyan"
                                                     , " - white" }

                    -- Set `vs` as a value multivalued parameter and a `v` single value type
                    -- generally placed last, used to get a list of variable parameters
                ,   {}
                ,   {nil, "contents", "vs", nil, "The info contents." }
                }
            }
	
target("npackdg")
	on_load(function (target)
        local appveyor = io.readfile("../appveyor.yml") 
		local version = string.trim(appveyor:split("\n")[1]:split(":")[2]:split(".{")[1])
		print("v:", version)
		target:set("version", version)
		target:add("defines", "NPACKD_VERSION=\"" .. version .. "\"")
		local parts = version:split(".")
    end)
    add_includedirs("C:/builds/quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static/include")
	add_includedirs(QT .. "/include");
	add_includedirs(QT .. "/include/QtSql");
	add_includedirs(QT .. "/include/QtCore");
	add_includedirs(QT .. "/share/qt5/mkspecs/win32-g++");
	add_includedirs(QT .. "/include/QtXml");
	add_includedirs(QT .. "/include/QtWidgets");
	add_includedirs(QT .. "/include/QtGui");
	add_includedirs(QT .. "/include/QtWinExtras");
	add_rules("qt.widgetapp_static")
	add_rules("qt.qrc")
    add_files("src/*.cpp")
    add_files("src/*.c")
    add_files("src/*.ui")
    add_files("src/*.h")
    add_files("src/*.qrc")
	add_files("$(buildir)/app.rc")
    set_configvar("NPACKD_VERSION_MAJOR", "1")
    set_configvar("NPACKD_VERSION_MINOR", "27")
    set_configvar("NPACKD_VERSION_PATCH", "0")
    set_configvar("NPACKD_VERSION_TWEAK", "1")
    set_configvar("OUTPUT_FILE_NAME", "npackdg.exe")
    add_configfiles("src/app.rc.in")
    add_configfiles("src/npackdg.manifest", {copyonly = true})
    add_configfiles("src/app.ico", {copyonly = true})
	add_defines("UNICODE", "_UNICODE")
	add_defines("QT_CORE_LIB", "QT_NO_DEBUG", "QT_SQL_LIB", "QT_XML_LIB", "QUAZIP_STATIC=1", "_WIN32_WINNT=0x0600", "NDEBUG", "QT_LINK_STATIC");
	add_cxflags("-static -static-libstdc++", "-static-libgcc", 
			"-g", "-Os", "-Wall", "-Winit-self", "-Wwrite-strings", 
			"-Wextra", "-Wno-long-long", "-Wno-overloaded-virtual", "-Wno-missing-field-initializers", "-Wno-unused-parameter", "-Wno-unknown-pragmas", "-Wno-cast-function-type", 
			"-Wno-unused-but-set-parameter", "-Wno-error=cast-qual", "-Wno-unused-local-typedefs", "-Wno-unused-variable", "-std=gnu++11");
	add_ldflags("-static", "-static-libstdc++", "-static-libgcc", "-g", "-Os", "-Wl,--subsystem,windows:6.1", "-Wl,-Map,build/npackdg.map", "-mwindows", "-Wl,--major-image-version,0,--minor-image-version,0", "-Wl,-O2", "-Wl,-s");
	add_linkdirs(QT .. "/share/qt5/plugins/platforms");
	add_linkdirs(QT .. "/share/qt5/plugins/imageformats");
	add_linkdirs(QT .. "/share/qt5/plugins/sqldrivers");
	add_linkdirs(QT .. "/share/qt5/plugins/styles");
	add_linkdirs(QT .. "/lib/");
	add_linkdirs("C:/builds/quazip-dev-x86_64-w64_seh_posix_8.2-qt_5.12-static/lib");
	add_links("quazip")
	add_links("qwindowsvistastyle")
	add_links("Qt5widgets")
	add_links("qwindows")
	add_links("Qt5VulkanSupport")
	add_links("Qt5Gui")
	add_links("Qt5WinExtras")
	add_links("Qt5FontDatabaseSupport")
	add_links("qsqlite")
	add_links("Qt5sql")
	add_links("Qt5xml")
	add_links("Qt5Core")
	add_links("qicns")
	add_links("qico")
	add_links("qjpeg")
	add_links("qgif")
	add_links("qtga")
	add_links("qtiff")
	add_links("qwbmp")
	add_links("qwebp")
	add_links("mingwex")
	add_links("Qt5ThemeSupport")
	add_links("Qt5EventDispatcherSupport")
	add_links("Qt5FontDatabaseSupport")
	add_links("Qt5PlatformCompositorSupport")
	add_links("Qt5WindowsUIAutomationSupport")
	add_links("qdirect2d")
	add_links("jasper")
	add_links("icuin")
	add_links("icuuc")
	add_links("icudt")
	add_links("icutu")
	add_links("qtpcre2")
	add_links("qtharfbuzz")
	add_links("qtfreetype")
	add_links("qtlibpng")
	add_links("jpeg")
	add_links("zstd")
	add_links("z")
	add_links("imm32")
	add_links("winmm")
	add_links("glu32")
	add_links("mpr")
	add_links("userenv")
	add_links("wtsapi32")
	add_links("opengl32")
	add_links("ole32")
	add_links("uuid")
	add_links("wininet")
	add_links("psapi")
	add_links("version")
	add_links("shlwapi")
	add_links("msi")
	add_links("netapi32")
	add_links("Ws2_32")
	add_links("UxTheme")
	add_links("Dwmapi")
	add_links("taskschd")
	add_links("oleaut32")
	
