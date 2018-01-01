cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdg CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/" "$ENV{QTDIR}" "${QTDIR}" "$ENV{QTDIR}/lib/cmake" "${QTDIR}/lib/cmake")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")

LIST(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}\\qtbase\\lib\\cmake\\Qt5") 
 
if(WIN32)
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif()

# CXX_STANDARD property is supported from cmake 3.1, we have to define -std with old cmake
if(NOT MSVC)
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} LESS 3.1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  else()
    set(CMAKE_CXX_STANDARD 11)
  endif ()
endif()

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
link_directories("$ENV{QTDIR}\\qtbase\\lib") 
link_directories("$ENV{QTDIR}\\qtbase\\plugins\\platforms")
link_directories("$ENV{QTDIR}\\qtbase\\plugins\\imageformats")

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc -g -Os")

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

find_package(QuaZip REQUIRED)

readVersion("../appveyor.yml")

file(GLOB RESOURCE_IMAGES "src/*.png")
file(COPY ${RESOURCE_IMAGES} src/wpmcpp.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(WPMCPP_SOURCES
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
    src/detectfile.cpp
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
    src/scandiskthirdpartypm.cpp
    src/mysqlquery.cpp
    src/repositoryxmlhandler.cpp
    src/cbsthirdpartypm.cpp
    src/visiblejobs.cpp
    src/progresstree2.cpp
    src/downloadsizefinder.cpp
    src/clprocessor.cpp
    src/exportrepositoryframe.cpp
    src/wpmcpp_plugin_import.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.qrc
)
set(WPMCPP_HEADERS
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
    src/detectfile.h
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
    src/scandiskthirdpartypm.h
    src/mysqlquery.h
    src/repositoryxmlhandler.h
    src/cbsthirdpartypm.h
    src/msoav2.h
    src/visiblejobs.h
    src/clprocessor.h
    src/progresstree2.h
    src/downloadsizefinder.h
    src/concurrent.h
    src/stable.h
    src/exportrepositoryframe.h
)
set(WPMCPP_FORMS
    src/mainwindow.ui
    src/packageversionform.ui
    src/licenseform.ui
    src/messageframe.ui
    src/settingsframe.ui
    src/packageframe.ui
    src/mainframe.ui
    src/exportrepositoryframe.ui
)
set(WPMCPP_TRANSLATIONS
    src/wpmcpp_es.ts
    src/wpmcpp_ru.ts
    src/wpmcpp_fr.ts
    src/wpmcpp_de.ts
)

set(OUTPUT_FILE_NAME "npackdg.exe")
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.rc @ONLY)
file(COPY src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
file(COPY src/wpmcpp.manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND WPMCPP_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.rc)

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/npackd.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql widgets winextras LinguistTools REQUIRED)
add_executable(npackdg WIN32
    ${WPMCPP_SOURCES}
    ${WPMCPP_HEADERS}
    ${WPMCPP_FORMS}
)
qt5_add_translation(WPMCPP_TRANSLATIONS_QM ${WPMCPP_TRANSLATIONS})
qt5_use_modules(npackdg Gui Core Xml Sql Widgets WinExtras)
target_link_libraries(npackdg
    ${QUAZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    
    qwindows
    Qt5PlatformSupport 
    qtfreetype
    
    qdds 
    qicns 
    qico 
    qjp2 
    qmng 
    qtga 
    qtiff 
    qwbmp 
    qwebp 
    
    imm32
    winmm 
    glu32 
    mpr
    
    qtharfbuzzng
    qtpcre
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
)
target_include_directories(npackdg PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_compile_definitions(npackdg PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS npackdg DESTINATION ${CMAKE_INSTALL_PREFIX})
if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.dll ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.pdb ${_qt5Core_install_prefix}/bin/Qt5Guid.dll ${_qt5Core_install_prefix}/bin/Qt5Guid.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll ${_qt5Core_install_prefix}/bin/Qt5Widgets.dll ${_qt5Core_install_prefix}/bin/Qt5Gui.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
install(FILES CrystalIcons_LICENSE.txt LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

file(TO_CMAKE_PATH "$ENV{EXEPROXY}" EXEPROXY)
install(FILES ${EXEPROXY}/exeproxy.exe DESTINATION ${CMAKE_INSTALL_PREFIX})

# todo
#   cd $(WHERE) && "$(DRMEMORY)\bin\drmemory.exe" -no_follow_children -report_leak_max 100000 -no_callstack_use_top_fp -- wpmcpp.exe

# todo memcheck
# memcheck: compile
#   cd $(WHERE) && "$(DRMEMORY)\bin\drmemory.exe" -light -count_leaks -no_follow_children -no_callstack_use_top_fp -- wpmcpp.exe

