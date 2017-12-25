cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdg CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/" "$ENV{QTDIR}" "${QTDIR}" "$ENV{QTDIR}/lib/cmake" "${QTDIR}/lib/cmake") 

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

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc" )

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /wd4251")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Oi")
  if(CMAKE_CL_64)
	set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
	set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /bigobj")
  endif()
  if(MSVC_VERSION GREATER 1500 OR MSVC_VERSION EQUAL 1500)
    option(NPACKD_BUILD_MSVC_MP "Enable build with multiple processes in Visual Studio" TRUE)
  else()
    set(NPACKD_BUILD_MSVC_MP FALSE CACHE BOOL "Compiler option /MP requires at least Visual Studio 2008 (VS9) or newer" FORCE)
  endif()
  if(NPACKD_BUILD_MSVC_MP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  endif()
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DDLL_EXPORT -DUNICODE -D_UNICODE)
endif()

if(MINGW)
  add_definitions(-D_WIN32_WINNT=0x0600)
  if(NOT NPACKD_PLATFORM_X64)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=i686")
  endif ()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fpermissive -fno-tree-slp-vectorize -Wno-error=cast-qual -Wno-unused-local-typedefs")
endif()

if((CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC) AND NOT MINGW)
  add_definitions( -Wall -pedantic -fPIC )
  check_cxx_compiler_flag(-fvisibility=hidden NPACKD_GCC_VISIBILITY)
  if(NPACKD_GCC_VISIBILITY)
    execute_process(COMMAND ${CMAKE_CXX_COMPILER} -dumpversion OUTPUT_VARIABLE NPACKD_GCC_VERSION)
    message(STATUS "Detected g++ ${NPACKD_GCC_VERSION}")
    message(STATUS "Enabling GCC visibility flags")
    set(NPACKD_GCC_VISIBILITY_FLAGS "-fvisibility=hidden")
    set(XCODE_ATTRIBUTE_GCC_SYMBOLS_PRIVATE_EXTERN "YES")
    string(TOLOWER "${CMAKE_BUILD_TYPE}" NPACKD_BUILD_TYPE)
    if(NPACKD_BUILD_TYPE STREQUAL "debug" AND NPACKD_GCC_VERSION VERSION_LESS "4.2")
      message(STATUS "Skipping -fvisibility-inlines-hidden due to possible bug in g++ < 4.2")
    else()
      if(APPLE)
        message(STATUS "Skipping -fvisibility-inlines-hidden due to linker issues")
        set(XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN[arch=x86_64] "YES")
      else()
        set(NPACKD_VISIBILITY_FLAGS "${NPACKD_GCC_VISIBILITY_FLAGS} -fvisibility-inlines-hidden")
        set(XCODE_ATTRIBUTE_GCC_INLINES_ARE_PRIVATE_EXTERN "YES")
      endif()
    endif()
  endif()
  if(NPACKD_PLATFORM_X64 AND NOT APPLE)
    add_definitions(-fPIC)
  endif()
endif()

# prefer static libraries if making import tool
if(BUILD_IMPORT_TOOL_FOR_DISTRIBUTION AND (CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC))
  SET(CMAKE_FIND_LIBRARY_SUFFIXES .a ${CMAKE_FIND_LIBRARY_SUFFIXES})
endif()

# postfix for debug builds
if(NOT APPLE)
  set(CMAKE_DEBUG_POSTFIX "d")
endif ()

find_package(QuaZip REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(STRINGS "Version.txt" READ_BUILD_NUMBER)
string(REGEX MATCHALL "[0-9]+" _versionComponents "${READ_BUILD_NUMBER}")
list(LENGTH _versionComponents _len)
if (${_len} GREATER 0)
	list(GET _versionComponents 0 NPACKD_VERSION_MAJOR)
else()
	set(NPACKD_VERSION_MAJOR 0)
endif()
if (${_len} GREATER 1)
	list(GET _versionComponents 1 NPACKD_VERSION_MINOR)
else()
	set(NPACKD_VERSION_MINOR 0)
endif()
if (${_len} GREATER 2)
	list(GET _versionComponents 2 NPACKD_VERSION_PATCH)
else()
	set(NPACKD_VERSION_PATCH 0)
endif()
if (${_len} GREATER 3)
	list(GET _versionComponents 3 NPACKD_VERSION_TWEAK)
else()
	set(NPACKD_VERSION_TWEAK 0)
endif()

set(NPACKD_VERSION "${NPACKD_VERSION_MAJOR}.${NPACKD_VERSION_MINOR}.${NPACKD_VERSION_PATCH}.${NPACKD_VERSION_TWEAK}")

message(STATUS "Build npackd v${NPACKD_VERSION}")

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
configure_file(${CMAKE_SOURCE_DIR}/../cmake/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.rc @ONLY)
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
install(FILES ${EXEPROXY}\\exeproxy.exe DESTINATION ${CMAKE_INSTALL_PREFIX})

# todo
# 	cd $(WHERE) && "$(DRMEMORY)\bin\drmemory.exe" -no_follow_children -report_leak_max 100000 -no_callstack_use_top_fp -- wpmcpp.exe

# todo memcheck
# memcheck: compile
# 	cd $(WHERE) && "$(DRMEMORY)\bin\drmemory.exe" -light -count_leaks -no_follow_children -no_callstack_use_top_fp -- wpmcpp.exe

# todo debug version
#	copy "$(DRMINGW)\bin\exchndl.dll" $(WHERE)\zip-debug
#	copy "$(DRMINGW)\bin\mgwhelp.dll" $(WHERE)\zip-debug
#	copy "$(DRMINGW)\bin\dbghelp.dll" $(WHERE)\zip-debug
#	copy "$(DRMINGW)\bin\symsrv.dll" $(WHERE)\zip-debug
#	copy "$(DRMINGW)\bin\symsrv.yes" $(WHERE)\zip-debug

#todo
# 	"$(MINGW)\bin\strip.exe" $(WHERE)\zip\npackdg.exe

# todo
# -Wl,-Map,wpmcpp_release.map

# todo
# -Os

# these 2 options can be used to add the debugging information to the "release"
# build
# QMAKE_CXXFLAGS_RELEASE += -g
# QMAKE_LFLAGS_RELEASE -= -Wl,-s
