cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdcl CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

if(WIN32)
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif()

if(NOT MSVC)
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")

  if(${NPACKD_FORCE_STATIC})
      SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc")
  endif()

  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -Os")
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
    ../npackdg/src/packageutils.cpp
    ../npackdg/src/wuathirdpartypm.cpp
    ../npackdg/src/wuapi_i.c
    ../npackdg/src/comobject.cpp
    ../npackdg/src/dismthirdpartypm.cpp
    ../npackdg/src/sqlutils.cpp
    ../npackdg/src/dag.cpp
    ../npackdg/src/deptask.cpp
    ../npackdg/src/threadpool.cpp
    ../npackdg/src/lockedfiles.cpp
    src/commandlinemessagehandler.cpp
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
    ../npackdg/src/packageutils.h
    ../npackdg/src/wuathirdpartypm.h
    ../npackdg/src/wuapi.h
    ../npackdg/src/comobject.h
    ../npackdg/src/dismthirdpartypm.h
    ../npackdg/src/sqlutils.h
    ../npackdg/src/dag.h
    ../npackdg/src/deptask.h
    ../npackdg/src/threadpool.h
    ../npackdg/src/lockedfiles.h
    src/commandlinemessagehandler.h
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

link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\platforms")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\imageformats")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\sqldrivers")
link_directories("${Qt5_DIR}\\..\\..")

add_executable(npackdcl
    ${NPACKDCL_SOURCES}
    ${NPACKDCL_HEADERS}
)

# libraries listed here like 'icuin' are necessary for static builds
SET(NPACKDCL_LIBRARIES ${QUAZIP_LIBRARIES} ${ZLIB_LIBRARIES})

if(${NPACKD_FORCE_STATIC})
    SET(NPACKDCL_LIBRARIES ${NPACKDCL_LIBRARIES} qsqlite)
endif()

SET(NPACKDCL_LIBRARIES ${NPACKDCL_LIBRARIES} Qt5::Sql Qt5::Xml Qt5::Core)

if(${NPACKD_FORCE_STATIC})
    SET(NPACKDCL_LIBRARIES ${NPACKDCL_LIBRARIES} mingwex qtpcre2 icuin icuuc icudt icutu zstd z)
endif()

SET(NPACKDCL_LIBRARIES ${NPACKDCL_LIBRARIES} userenv winmm ole32 uuid wininet psapi version shlwapi msi netapi32 Ws2_32 taskschd)

target_link_libraries(npackdcl ${NPACKDCL_LIBRARIES})
target_include_directories(npackdcl PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../npackdg/src)
target_compile_definitions(npackdcl PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)
if(${NPACKD_ADMIN})
  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=1)
else()
  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=0)
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
