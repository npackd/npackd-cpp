cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(clu CXX C)

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

SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static" )

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

readVersion("../wpmcpp/Version.txt")

set(CLU_SOURCES
    src/main.cpp
    src/app.cpp
    ../wpmcpp/src/clprogress.cpp
    ../wpmcpp/src/package.cpp
    ../wpmcpp/src/license.cpp
    ../wpmcpp/src/dependency.cpp
    ../wpmcpp/src/detectfile.cpp
    ../wpmcpp/src/abstractrepository.cpp
    ../wpmcpp/src/repository.cpp
    ../wpmcpp/src/packageversion.cpp
    ../wpmcpp/src/packageversionfile.cpp
    ../wpmcpp/src/installedpackageversion.cpp
    ../wpmcpp/src/windowsregistry.cpp
    ../wpmcpp/src/abstractthirdpartypm.cpp
    ../wpmcpp/src/controlpanelthirdpartypm.cpp
    ../wpmcpp/src/commandline.cpp
    ../wpmcpp/src/wpmutils.cpp
    ../wpmcpp/src/job.cpp
    ../wpmcpp/src/hrtimer.cpp
    ../wpmcpp/src/version.cpp
    ../wpmcpp/src/installedpackages.cpp
    ../wpmcpp/src/installoperation.cpp
    ../wpmcpp/src/dbrepository.cpp
    ../wpmcpp/src/downloader.cpp
    ../wpmcpp/src/repositoryxmlhandler.cpp
    ../wpmcpp/src/installedpackagesthirdpartypm.cpp
    ../wpmcpp/src/wellknownprogramsthirdpartypm.cpp
    ../wpmcpp/src/msithirdpartypm.cpp
    ../wpmcpp/src/mysqlquery.cpp
        )
set(CLU_HEADERS
    src/app.h
    ../wpmcpp/src/clprogress.h
    ../wpmcpp/src/package.h
    ../wpmcpp/src/license.h
    ../wpmcpp/src/dependency.h
    ../wpmcpp/src/detectfile.h
    ../wpmcpp/src/abstractrepository.h
    ../wpmcpp/src/repository.h
    ../wpmcpp/src/packageversion.h
    ../wpmcpp/src/packageversionfile.h
    ../wpmcpp/src/installedpackageversion.h
    ../wpmcpp/src/windowsregistry.h
    ../wpmcpp/src/abstractthirdpartypm.h
    ../wpmcpp/src/controlpanelthirdpartypm.h
    ../wpmcpp/src/commandline.h
    ../wpmcpp/src/wpmutils.h
    ../wpmcpp/src/job.h
    ../wpmcpp/src/hrtimer.h
    ../wpmcpp/src/version.h
    ../wpmcpp/src/installedpackages.h
    ../wpmcpp/src/installoperation.h
    ../wpmcpp/src/dbrepository.h
    ../wpmcpp/src/downloader.h
    ../wpmcpp/src/repositoryxmlhandler.h
    ../wpmcpp/src/installedpackagesthirdpartypm.h
    ../wpmcpp/src/wellknownprogramsthirdpartypm.h
    ../wpmcpp/src/msithirdpartypm.h
    ../wpmcpp/src/mysqlquery.h
)

set(OUTPUT_FILE_NAME "clu.exe")
configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/clu.rc @ONLY)
file(COPY ../wpmcpp/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CLU_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/clu.rc)

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ncl.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql REQUIRED)
add_executable(clu
    ${CLU_SOURCES}
    ${CLU_HEADERS}
)
#qt5_add_translation(WPMCPP_TRANSLATIONS_QM ${WPMCPP_TRANSLATIONS})
qt5_use_modules(clu Core Xml Sql)
target_link_libraries(clu
    ${QUAZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    qtpcre
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
target_include_directories(clu PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/../wpmcpp/src)
target_compile_definitions(clu PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS clu DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()

# todo
# contains(QT_ARCH, i386) {
#     QMAKE_CXXFLAGS_RELEASE += -flto
# }
