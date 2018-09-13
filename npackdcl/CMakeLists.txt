cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdcl CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/" "$ENV{QTDIR}" "${QTDIR}" "$ENV{QTDIR}/lib/cmake" "${QTDIR}/lib/cmake")

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
else()
  if (MSVC_VERSION LESS "1800")
    message(FATAL_ERROR "No suitable compiler! C++11 required!")
  endif()
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} GREATER 3.0)
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

link_directories("$ENV{QTDIR}\\share\\qt5\\plugins\\platforms")
link_directories("$ENV{QTDIR}\\share\\qt5\\plugins\\imageformats")
link_directories("$ENV{QTDIR}\\share\\qt5\\plugins\\sqldrivers")

if(NOT MSVC)
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static -g -Os")
endif()

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

find_package(QuaZip REQUIRED)

readVersion("../appveyor.yml")

set(NPACKDCL_SOURCES
    ../npackdg/src/visiblejobs.cpp
    ../npackdg/src/repository.cpp
    ../npackdg/src/version.cpp
    ../npackdg/src/packageversionfile.cpp
    ../npackdg/src/package.cpp
    ../npackdg/src/packageversion.cpp
    ../npackdg/src/job.cpp
    ../npackdg/src/installoperation.cpp
    ../npackdg/src/dependency.cpp
    ../npackdg/src/wpmutils.cpp
    ../npackdg/src/downloader.cpp
    ../npackdg/src/license.cpp
    ../npackdg/src/windowsregistry.cpp
    ../npackdg/src/detectfile.cpp
    ../npackdg/src/commandline.cpp
    ../npackdg/src/installedpackages.cpp
    ../npackdg/src/installedpackageversion.cpp
    ../npackdg/src/clprogress.cpp
    ../npackdg/src/dbrepository.cpp
    ../npackdg/src/abstractrepository.cpp
    ../npackdg/src/abstractthirdpartypm.cpp
    ../npackdg/src/msithirdpartypm.cpp
    ../npackdg/src/controlpanelthirdpartypm.cpp
    ../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../npackdg/src/hrtimer.cpp
    ../npackdg/src/repositoryxmlhandler.cpp
    ../npackdg/src/mysqlquery.cpp
    ../npackdg/src/installedpackagesthirdpartypm.cpp
    ../npackdg/src/urlinfo.cpp
    src/main.cpp
    src/app.cpp
)
set(NPACKDCL_HEADERS
    ../npackdg/src/visiblejobs.h
    ../npackdg/src/repository.h
    ../npackdg/src/version.h
    ../npackdg/src/packageversionfile.h
    ../npackdg/src/package.h
    ../npackdg/src/packageversion.h
    ../npackdg/src/job.h
    ../npackdg/src/installoperation.h
    ../npackdg/src/dependency.h
    ../npackdg/src/wpmutils.h
    ../npackdg/src/downloader.h
    ../npackdg/src/license.h
    ../npackdg/src/windowsregistry.h
    ../npackdg/src/detectfile.h
    ../npackdg/src/installedpackages.h
    ../npackdg/src/installedpackageversion.h
    ../npackdg/src/commandline.h
    ../npackdg/src/clprogress.h
    ../npackdg/src/dbrepository.h
    ../npackdg/src/abstractrepository.h
    ../npackdg/src/abstractthirdpartypm.h
    ../npackdg/src/msithirdpartypm.h
    ../npackdg/src/controlpanelthirdpartypm.h
    ../npackdg/src/wellknownprogramsthirdpartypm.h
    ../npackdg/src/hrtimer.h
    ../npackdg/src/repositoryxmlhandler.h
    ../npackdg/src/mysqlquery.h
    ../npackdg/src/installedpackagesthirdpartypm.h
    ../npackdg/src/urlinfo.h
    src/app.h
)

option(NPACKD_ADMIN "Force admin right on program" TRUE)
set(OUTPUT_FILE_NAME "npackdcl.exe")
configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdcl.rc @ONLY)
file(COPY ../npackdg/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND NPACKDCL_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/npackdcl.rc)

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/npackdcl.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql REQUIRED)
add_executable(npackdcl
    ${NPACKDCL_SOURCES}
    ${NPACKDCL_HEADERS}
)
target_link_libraries(npackdcl
    ${QUAZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    Qt5::Sql Qt5::Xml Qt5::Core
    winmm
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
target_include_directories(npackdcl PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../npackdg/src)
target_compile_definitions(npackdcl PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)
if(${NPACKD_ADMIN})
  target_compile_definitions(npackdg PRIVATE -D NPACKD_ADMIN=1)
else()
  target_compile_definitions(npackdg PRIVATE -D NPACKD_ADMIN=0)
endif()

install(TARGETS npackdcl DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ../CrystalIcons_LICENSE.txt ../LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()

add_subdirectory(tests)
add_subdirectory(ftests)
