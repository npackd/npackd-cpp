cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackd-cpp)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/" "$ENV{QTDIR}" "${QTDIR}" "$ENV{QTDIR}/lib/cmake" "${QTDIR}/lib/cmake") 

LIST(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}\\qtbase\\lib\\cmake\\Qt5") 
 
if(WIN32)
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
endif()

# CXX_STANDARD property is supported from cmake 3.1, we have to define -std with old cmake
set(CMAKE_CXX_STANDARD 11)
if(NOT MSVC)
  if(${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION} LESS 3.1)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")
  endif()
  SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static-libstdc++ -static-libgcc -static -g -Os")
  SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")
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

include(CheckCXXCompilerFlag)
include(cmake/Common.cmake)
add_definitions(-DUNICODE -D_UNICODE)
readVersion("appveyor.yml")

option(NPACKD_ADMIN "Force admin right on program" TRUE)
option(NPACKD_BUILD_TESTS "Build tests" TRUE)
option(NPACKED_BUILD_CLU "Build command line utility" TRUE)
option(NPACKED_BUILD_NCL "Build command line interface for the Npackd software package manager" TRUE)
option(NPACKED_BUILD_NPACKDG "Build the graphical interface for the Npackd software package manager" TRUE)
option(NPACKED_FORCE_STATIC_QT "Force generator to link Qt static" FALSE)
mark_as_advanced(NPACKED_FORCE_STATIC_QT)

find_package(QuaZip REQUIRED)
find_package(Qt5 COMPONENTS core xml sql REQUIRED)
if(${NPACKED_FORCE_STATIC_QT})
  find_package(ZLIB REQUIRED)
  link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\platforms")
  link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\imageformats")
  link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\sqldrivers")
  link_directories("${Qt5_DIR}\\..\\..\\..\\share\\qt5\\plugins\\styles")
  link_directories("${Qt5_DIR}\\..\\..\\..\\plugins\\platforms")
  link_directories("${Qt5_DIR}\\..\\..\\..\\plugins\\imageformats")
  link_directories("${Qt5_DIR}\\..\\..\\..\\plugins\\sqldrivers")
  link_directories("${Qt5_DIR}\\..\\..\\..\\plugins\\styles")
  link_directories("${Qt5_DIR}\\..\\..")
endif()
if(WIN32)
  get_target_property(_qmake_executable Qt5::qmake IMPORTED_LOCATION)
  get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
  find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")
endif()
if(MSVC)
  set(QT_USE_IMPORTED_TARGETS TRUE)
  set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
endif() 

add_definitions(-DUNICODE -D_UNICODE ${QUAZIP_DEFINE} -DNPACKD_VERSION="${NPACKD_VERSION}")
if(${NPACKD_ADMIN})
  add_definitions(-DNPACKD_ADMIN=1)
else()
  add_definitions(-DNPACKD_ADMIN=0)
endif()

set(NPACKED_BASE_LIBS
  ${QUAZIP_LIBRARIES}
  Qt5::Sql
  Qt5::Xml
  Qt5::Core
  winmm
  ole32
  uuid
  wininet
  psapi
  version
  userenv
  shlwapi
  msi
  netapi32
  Ws2_32
  taskschd
)
include_directories(${QUAZIP_INCLUDE_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/npackdg/src)
if(${ZLIB_FOUND})
  include_directories(${ZLIB_INCLUDE_DIRS})
  list(APPEND NPACKED_BASE_LIBS ${ZLIB_LIBRARIES})
endif()
if(${NPACKED_FORCE_STATIC_QT})
  list(APPEND NPACKED_BASE_LIBS
    qsqlite
    qtpcre2
  )
  add_definitions(-DQT_LINK_STATIC=1)
else()
  add_definitions(-DQT_LINK_STATIC=0)
endif()
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES CrystalIcons_LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

if(${NPACKED_BUILD_CLU})
  add_subdirectory(clu)
endif()
if(${NPACKED_BUILD_NCL})
  add_subdirectory(npackdcl)
endif()
if(${NPACKED_BUILD_NPACKDG})
  add_subdirectory(npackdg)
endif()

