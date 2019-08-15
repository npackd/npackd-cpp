cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdg CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

# build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt5_DIR=C:\msys64\mingw64\qt5-static\lib\cmake\Qt5
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

if(WIN32)
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif()

set(CMAKE_CXX_STANDARD 11)

include(CheckTypeSize)
CHECK_TYPE_SIZE("void*" OSMSCOUT_PTR_SIZE BUILTIN_TYPES_ONLY)
if(OSMSCOUT_PTR_SIZE EQUAL 8)
  set(NPACKD_PLATFORM_X64 TRUE)
  set(BITS 64)
else()
  set(NPACKD_PLATFORM_X64 FALSE)
  set(BITS 32)
endif()

add_definitions(-DUNICODE -D_UNICODE)

#  -Wl,--major-os-version,6 -Wl,--minor-os-version,1
if(NOT MSVC)
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc -g -Os -Wl,--subsystem,windows:6.1")
endif()

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

find_package(QuaZip REQUIRED)

readVersion("../appveyor.yml")

file(GLOB RESOURCE_IMAGES "src/*.png")
file(COPY ${RESOURCE_IMAGES} src/npackdg.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(NPACKDG_SOURCES
    src/main.cpp
    src/mainwindow.cpp
    src/packageversion.cpp
    src/repository.cpp
    src/job.cpp
    src/downloader.cpp
    src/wpmutils.cpp
    src/package.cpp
    src/packageversionfile.cpp
    src/version.cpp
    src/dependency.cpp
    src/fileloader.cpp
    src/installoperation.cpp
    src/packageversionform.cpp
    src/license.cpp
    src/licenseform.cpp
    src/windowsregistry.cpp
    src/uiutils.cpp
    src/commandline.cpp
    src/messageframe.cpp
    src/settingsframe.cpp
    src/packageframe.cpp
    src/selection.cpp
    src/hrtimer.cpp
    src/clprogress.cpp
    src/mainframe.cpp
    src/dbrepository.cpp
    src/installedpackages.cpp
    src/installedpackageversion.cpp
    src/abstractrepository.cpp
    src/packageitemmodel.cpp
    src/abstractthirdpartypm.cpp
    src/controlpanelthirdpartypm.cpp
    src/msithirdpartypm.cpp
    src/wellknownprogramsthirdpartypm.cpp
    src/installedpackagesthirdpartypm.cpp
    src/flowlayout.cpp
    src/mysqlquery.cpp
    src/repositoryxmlhandler.cpp
    src/visiblejobs.cpp
    src/progresstree2.cpp
    src/downloadsizefinder.cpp
    src/clprocessor.cpp
    src/exportrepositoryframe.cpp
    src/npackdg_plugin_import.cpp
    src/urlinfo.cpp
    src/asyncdownloader.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/npackdg.qrc
)
set(NPACKDG_HEADERS
    src/mainwindow.h
    src/packageversion.h
    src/repository.h
    src/job.h
    src/downloader.h
    src/wpmutils.h
    src/package.h
    src/packageversionfile.h
    src/version.h
    src/dependency.h
    src/fileloader.h
    src/installoperation.h
    src/packageversionform.h
    src/license.h
    src/licenseform.h
    src/windowsregistry.h
    src/uiutils.h
    src/commandline.h
    src/messageframe.h
    src/settingsframe.h
    src/mstask.h
    src/packageframe.h
    src/selection.h
    src/hrtimer.h
    src/clprogress.h
    src/mainframe.h
    src/dbrepository.h
    src/installedpackages.h
    src/installedpackageversion.h
    src/abstractrepository.h
    src/packageitemmodel.h
    src/abstractthirdpartypm.h
    src/controlpanelthirdpartypm.h
    src/msithirdpartypm.h
    src/wellknownprogramsthirdpartypm.h
    src/installedpackagesthirdpartypm.h
    src/flowlayout.h
    src/mysqlquery.h
    src/repositoryxmlhandler.h
    src/msoav2.h
    src/visiblejobs.h
    src/clprocessor.h
    src/progresstree2.h
    src/downloadsizefinder.h
    src/concurrent.h
    src/stable.h
    src/exportrepositoryframe.h
    src/urlinfo.h
    src/asyncdownloader.h
)
set(NPACKDG_FORMS
    src/mainwindow.ui
    src/packageversionform.ui
    src/licenseform.ui
    src/messageframe.ui
    src/settingsframe.ui
    src/packageframe.ui
    src/mainframe.ui
    src/exportrepositoryframe.ui
)
set(NPACKDG_TRANSLATIONS
    src/npackdg_es.ts
    src/npackdg_ru.ts
    src/npackdg_fr.ts
    src/npackdg_de.ts
)

option(NPACKD_ADMIN "Force admin right on program" TRUE)
set(OUTPUT_FILE_NAME "npackdg.exe")
file(COPY src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(${NPACKD_ADMIN})
  file(COPY src/npackdg.manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
  configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc @ONLY)
else()
  configure_file(../npackdcl/src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc @ONLY)
endif()
list(APPEND NPACKDG_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc)

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS Gui xml sql widgets WinExtras LinguistTools REQUIRED)

link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\platforms")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\imageformats")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\sqldrivers")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\styles")
link_directories("${Qt5_DIR}\\..\\..")

add_executable(npackdg WIN32
    ${NPACKDG_SOURCES}
    ${NPACKDG_HEADERS}
    ${NPACKDG_FORMS}
)
QT5_CREATE_TRANSLATION(qm_files src/ ${NPACKDG_TRANSLATIONS} OPTIONS -locations none)
qt5_add_translation(NPACKDG_TRANSLATIONS_QM ${NPACKDG_TRANSLATIONS})

# libraries listed here like 'icuin' are necessary in Qt 5.12 for static builds
target_link_libraries(npackdg
    ${QUAZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}

    qsqlite
    qicns
    qico
    qjpeg
    qgif
    qtga
    qtiff
    qwbmp
    qwebp
    Qt5::WinExtras qwindows Qt5VulkanSupport qwindowsvistastyle Qt5::Widgets Qt5FontDatabaseSupport
    Qt5::Gui Qt5::Sql Qt5::Xml Qt5::Core

    mingwex

    Qt5ThemeSupport
    Qt5EventDispatcherSupport
    Qt5FontDatabaseSupport
    Qt5PlatformCompositorSupport
    Qt5WindowsUIAutomationSupport
    qdirect2d

    jasper
    icuin
    icuuc
    icudt
    icutu
    qtpcre2
    qtharfbuzz
    qtfreetype
    qtlibpng
    jpeg

    imm32
    winmm 
    glu32 
    mpr
    
    userenv
    wtsapi32
    opengl32
    ole32
    uuid
    wininet
    psapi
    version
    shlwapi
    msi
    netapi32
    Ws2_32
    UxTheme
    Dwmapi
)
target_include_directories(npackdg PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_compile_definitions(npackdg PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)
if(${NPACKD_ADMIN})
  target_compile_definitions(npackdg PRIVATE -D NPACKD_ADMIN=1)
else()
  target_compile_definitions(npackdg PRIVATE -D NPACKD_ADMIN=0)
endif()

install(TARGETS npackdg DESTINATION ${CMAKE_INSTALL_PREFIX})
if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.dll ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.pdb ${_qt5Core_install_prefix}/bin/Qt5Guid.dll ${_qt5Core_install_prefix}/bin/Qt5Guid.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll ${_qt5Core_install_prefix}/bin/Qt5Widgets.dll ${_qt5Core_install_prefix}/bin/Qt5Gui.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
install(FILES ../CrystalIcons_LICENSE.txt ../LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

file(TO_CMAKE_PATH "$ENV{EXEPROXY}" EXEPROXY)
install(FILES ${EXEPROXY}/exeproxy.exe DESTINATION ${CMAKE_INSTALL_PREFIX})

