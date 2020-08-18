cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(tests CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../../cmake/")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")

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

include(../../cmake/Common.cmake)

find_package(QuaZip REQUIRED)

readVersion("../../appveyor.yml")

set(TESTS_SOURCES
    src/main.cpp
    ../../npackdg/src/visiblejobs.cpp
    ../../npackdg/src/repository.cpp
    ../../npackdg/src/version.cpp
    ../../npackdg/src/packageversionfile.cpp
    ../../npackdg/src/package.cpp
    ../../npackdg/src/packageversion.cpp
    ../../npackdg/src/job.cpp
    ../../npackdg/src/installoperation.cpp
    ../../npackdg/src/dependency.cpp
    ../../npackdg/src/wpmutils.cpp
    ../../npackdg/src/downloader.cpp
    ../../npackdg/src/license.cpp
    ../../npackdg/src/windowsregistry.cpp
    src/app.cpp
    ../../npackdg/src/commandline.cpp
    ../../npackdg/src/installedpackages.cpp
    ../../npackdg/src/installedpackageversion.cpp
    ../../npackdg/src/clprogress.cpp
    ../../npackdg/src/dbrepository.cpp
    ../../npackdg/src/abstractrepository.cpp
    ../../npackdg/src/abstractthirdpartypm.cpp
    ../../npackdg/src/msithirdpartypm.cpp
    ../../npackdg/src/controlpanelthirdpartypm.cpp
    ../../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../../npackdg/src/hrtimer.cpp
    ../../npackdg/src/repositoryxmlhandler.cpp
    ../../npackdg/src/mysqlquery.cpp
    ../../npackdg/src/installedpackagesthirdpartypm.cpp
    ../../npackdg/src/urlinfo.cpp
    ../../npackdg/src/packageutils.cpp
    ../../npackdg/src/wuathirdpartypm.cpp
    ../../npackdg/src/wuapi_i.c
    ../../npackdg/src/comobject.cpp
    ../../npackdg/src/dismthirdpartypm.cpp
)
set(TESTS_HEADERS
    ../../npackdg/src/visiblejobs.h
    ../../npackdg/src/repository.h
    ../../npackdg/src/version.h
    ../../npackdg/src/packageversionfile.h
    ../../npackdg/src/package.h
    ../../npackdg/src/packageversion.h
    ../../npackdg/src/job.h
    ../../npackdg/src/installoperation.h
    ../../npackdg/src/dependency.h
    ../../npackdg/src/wpmutils.h
    ../../npackdg/src/downloader.h
    ../../npackdg/src/license.h
    ../../npackdg/src/windowsregistry.h
    src/app.h
    ../../npackdg/src/installedpackages.h
    ../../npackdg/src/installedpackageversion.h
    ../../npackdg/src/commandline.h
    ../../npackdg/src/clprogress.h
    ../../npackdg/src/dbrepository.h
    ../../npackdg/src/abstractrepository.h
    ../../npackdg/src/abstractthirdpartypm.h
    ../../npackdg/src/msithirdpartypm.h
    ../../npackdg/src/controlpanelthirdpartypm.h
    ../../npackdg/src/wellknownprogramsthirdpartypm.h
    ../../npackdg/src/hrtimer.h
    ../../npackdg/src/repositoryxmlhandler.h
    ../../npackdg/src/mysqlquery.h
    ../../npackdg/src/installedpackagesthirdpartypm.h
    ../../npackdg/src/urlinfo.h
    ../../npackdg/src/packageutils.h
    ../../npackdg/src/wuathirdpartypm.h
    ../../npackdg/src/wuapi.h
    ../../npackdg/src/comobject.h
    ../../npackdg/src/dismthirdpartypm.cpp
)

set(OUTPUT_FILE_NAME "tests.exe")
file(COPY DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

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

# libraries listed here like 'icuin' are necessary for static builds
SET(TESTS_LIBRARIES ${QUAZIP_LIBRARIES} ${ZLIB_LIBRARIES})

if(${NPACKD_FORCE_STATIC})
    SET(TESTS_LIBRARIES ${TESTS_LIBRARIES} qsqlite)
endif()

SET(TESTS_LIBRARIES ${TESTS_LIBRARIES} Qt5::Sql Qt5::Test Qt5::Xml Qt5::Core)

if(${NPACKD_FORCE_STATIC})
    SET(TESTS_LIBRARIES ${TESTS_LIBRARIES} qtpcre2 icuin icuuc icudt icutu qtharfbuzz zstd z)
endif()

SET(TESTS_LIBRARIES ${TESTS_LIBRARIES} userenv winmm ole32 uuid wininet psapi version shlwapi msi netapi32 Ws2_32 taskschd)

add_executable(tests
    ${TESTS_SOURCES}
    ${TESTS_HEADERS}
)
target_link_libraries(tests ${TESTS_LIBRARIES})
target_include_directories(tests PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../../npackdg/src)
target_compile_definitions(tests PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS tests DESTINATION ${CMAKE_INSTALL_PREFIX})

if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
