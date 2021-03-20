cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(clu CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")
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
  endif()
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

set(CLU_SOURCES
    src/main.cpp
    src/app.cpp
    ../npackdg/src/clprogress.cpp
    ../npackdg/src/package.cpp
    ../npackdg/src/license.cpp
    ../npackdg/src/dependency.cpp
    ../npackdg/src/abstractrepository.cpp
    ../npackdg/src/repository.cpp
    ../npackdg/src/packageversion.cpp
    ../npackdg/src/packageversionfile.cpp
    ../npackdg/src/installedpackageversion.cpp
    ../npackdg/src/windowsregistry.cpp
    ../npackdg/src/abstractthirdpartypm.cpp
    ../npackdg/src/controlpanelthirdpartypm.cpp
    ../npackdg/src/commandline.cpp
    ../npackdg/src/wpmutils.cpp
    ../npackdg/src/job.cpp
    ../npackdg/src/hrtimer.cpp
    ../npackdg/src/version.cpp
    ../npackdg/src/installedpackages.cpp
    ../npackdg/src/installoperation.cpp
    ../npackdg/src/dbrepository.cpp
    ../npackdg/src/downloader.cpp
    ../npackdg/src/repositoryxmlhandler.cpp
    ../npackdg/src/installedpackagesthirdpartypm.cpp
    ../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../npackdg/src/msithirdpartypm.cpp
    ../npackdg/src/mysqlquery.cpp
    ../npackdg/src/packageutils.cpp
    ../npackdg/src/wuathirdpartypm.cpp
    ../npackdg/src/wuapi_i.c
    ../npackdg/src/comobject.cpp
    ../npackdg/src/sqlutils.cpp
    ../npackdg/src/dag.cpp
    ../npackdg/src/lockedfiles.cpp
    ../npackdg/src/threadpool.cpp
    ../npackdg/src/deptask.cpp
    ../npackdcl/src/commandlinemessagehandler.cpp
)
set(CLU_HEADERS
    src/app.h
    ../npackdg/src/clprogress.h
    ../npackdg/src/package.h
    ../npackdg/src/license.h
    ../npackdg/src/dependency.h
    ../npackdg/src/abstractrepository.h
    ../npackdg/src/repository.h
    ../npackdg/src/packageversion.h
    ../npackdg/src/packageversionfile.h
    ../npackdg/src/installedpackageversion.h
    ../npackdg/src/windowsregistry.h
    ../npackdg/src/abstractthirdpartypm.h
    ../npackdg/src/controlpanelthirdpartypm.h
    ../npackdg/src/commandline.h
    ../npackdg/src/wpmutils.h
    ../npackdg/src/job.h
    ../npackdg/src/hrtimer.h
    ../npackdg/src/version.h
    ../npackdg/src/installedpackages.h
    ../npackdg/src/installoperation.h
    ../npackdg/src/dbrepository.h
    ../npackdg/src/downloader.h
    ../npackdg/src/repositoryxmlhandler.h
    ../npackdg/src/installedpackagesthirdpartypm.h
    ../npackdg/src/wellknownprogramsthirdpartypm.h
    ../npackdg/src/msithirdpartypm.h
    ../npackdg/src/mysqlquery.h
    ../npackdg/src/packageutils.h
    ../npackdg/src/wuathirdpartypm.h
    ../npackdg/src/wuapi.h
    ../npackdg/src/comobject.h
    ../npackdg/src/sqlutils.h
    ../npackdg/src/dag.h
    ../npackdg/src/lockedfiles.h
    ../npackdg/src/threadpool.h
    ../npackdg/src/deptask.h
    ../npackdcl/src/commandlinemessagehandler.h
)

set(OUTPUT_FILE_NAME "clu.exe")
configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/clu.rc @ONLY)
file(COPY ../npackdg/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CLU_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/clu.rc)

if(MSVC)
    # Configure the template file that allows debugging
    set(QT_USE_IMPORTED_TARGETS TRUE)
    find_package(Qt5Core REQUIRED)
    set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ncl.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql REQUIRED)

link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\platforms")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\imageformats")
link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\sqldrivers")
link_directories("${Qt5_DIR}\\..\\..")

SET(CLU_LIBRARIES ${QUAZIP_LIBRARIES} ${ZLIB_LIBRARIES})

if(${NPACKD_FORCE_STATIC})
    SET(CLU_LIBRARIES ${CLU_LIBRARIES} qsqlite)
endif()

SET(CLU_LIBRARIES ${CLU_LIBRARIES} Qt5::Sql Qt5::Xml Qt5::Core)

if(${NPACKD_FORCE_STATIC})
    SET(CLU_LIBRARIES ${CLU_LIBRARIES} qtpcre2 zstd z)
endif()

SET(CLU_LIBRARIES ${CLU_LIBRARIES} userenv winmm ole32 uuid wininet psapi version shlwapi msi netapi32 Ws2_32 taskschd)

add_executable(clu
    ${CLU_SOURCES}
    ${CLU_HEADERS}
)
target_link_libraries(clu ${CLU_LIBRARIES})
target_include_directories(clu PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_CURRENT_SOURCE_DIR}/../npackdg/src ${CMAKE_CURRENT_SOURCE_DIR}/../npackdcl/src)
target_compile_definitions(clu PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS clu DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES ../LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

if(MSVC)
    set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
    set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
    install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
    install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
