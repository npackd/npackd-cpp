cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(ftests CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")

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

SET(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static -g -Os")

include(CheckCXXCompilerFlag)

include(../../cmake/Common.cmake)

find_package(QuaZip REQUIRED)

readVersion("../../appveyor.yml")

set(FTESTS_SOURCES
    src/app.cpp
    ../../npackdg/src/downloader.cpp
    ../../npackdg/src/commandline.cpp
    ../../npackdg/src/repositoryxmlhandler.cpp
    ../../npackdg/src/mysqlquery.cpp
    ../../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../../npackdg/src/abstractthirdpartypm.cpp
    ../../npackdg/src/msithirdpartypm.cpp
    ../../npackdg/src/installedpackagesthirdpartypm.cpp
    ../../npackdg/src/controlpanelthirdpartypm.cpp
    ../../npackdg/src/installoperation.cpp
    ../../npackdg/src/dependency.cpp
    ../../npackdg/src/packageversionfile.cpp
    ../../npackdg/src/dbrepository.cpp
    ../../npackdg/src/license.cpp
    ../../npackdg/src/repository.cpp
    ../../npackdg/src/job.cpp
    ../../npackdg/src/hrtimer.cpp
    ../../npackdg/src/package.cpp
    ../../npackdg/src/installedpackageversion.cpp
    ../../npackdg/src/abstractrepository.cpp
    ../../npackdg/src/version.cpp
    ../../npackdg/src/installedpackages.cpp
    ../../npackdg/src/windowsregistry.cpp
    ../../npackdg/src/packageversion.cpp
    ../../npackdg/src/wpmutils.cpp
    ../../npackdg/src/clprogress.cpp
    ../../npackdg/src/urlinfo.cpp
    src/main.cpp
)
set(FTESTS_HEADERS
    src/app.h
    ../../npackdg/src/downloader.h
    ../../npackdg/src/commandline.h
    ../../npackdg/src/repositoryxmlhandler.h
    ../../npackdg/src/mysqlquery.h
    ../../npackdg/src/wellknownprogramsthirdpartypm.h
    ../../npackdg/src/abstractthirdpartypm.h
    ../../npackdg/src/msithirdpartypm.h
    ../../npackdg/src/installedpackagesthirdpartypm.h
    ../../npackdg/src/controlpanelthirdpartypm.h
    ../../npackdg/src/installoperation.h
    ../../npackdg/src/dependency.h
    ../../npackdg/src/packageversionfile.h
    ../../npackdg/src/dbrepository.h
    ../../npackdg/src/license.h
    ../../npackdg/src/repository.h
    ../../npackdg/src/job.h
    ../../npackdg/src/hrtimer.h
    ../../npackdg/src/package.h
    ../../npackdg/src/installedpackageversion.h
    ../../npackdg/src/abstractrepository.h
    ../../npackdg/src/version.h
    ../../npackdg/src/installedpackages.h
    ../../npackdg/src/windowsregistry.h
    ../../npackdg/src/packageversion.h
    ../../npackdg/src/wpmutils.h
    ../../npackdg/src/clprogress.h
    ../../npackdg/src/urlinfo.h
)

set(OUTPUT_FILE_NAME "ftests.exe")

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ncl.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql test REQUIRED)

link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\platforms")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\imageformats")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\sqldrivers")
link_directories("${Qt5_DIR}\\..\\..")

add_executable(ftests
        ${FTESTS_SOURCES}
        ${FTESTS_HEADERS}
)
target_link_libraries(ftests
    ${QUAZIP_LIBRARIES}
    ${ZLIB_LIBRARIES}
    qsqlite
    Qt5::Sql Qt5::Test Qt5::Xml Qt5::Core
    qtpcre2
    icuin
    icuuc
    icudt
    icutu
    qtharfbuzz

    userenv
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
target_include_directories(ftests PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../npackdg/src)
target_compile_definitions(ftests PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS ftests DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES DESTINATION ${CMAKE_INSTALL_PREFIX})

if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
