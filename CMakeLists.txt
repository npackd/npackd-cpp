cmake_minimum_required(VERSION 3.10 FATAL_ERROR)

project(npackd-cpp)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_CXX_STANDARD_REQUIRED ON) # build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake/" "$ENV{QTDIR}" "${QTDIR}" "$ENV{QTDIR}/lib/cmake" "${QTDIR}/lib/cmake") 
list(APPEND CMAKE_PREFIX_PATH "$ENV{QTDIR}\\qtbase\\lib\\cmake\\Qt5") 
 
include(cmake/Common.cmake)

option(NPACKD_ADMIN "Force admin right on program" TRUE)
option(NPACKD_BUILD_TESTS "Build tests" TRUE)
option(NPACKD_BUILD_CLU "Build command line utility" TRUE)
option(NPACKD_BUILD_NCL "Build command line interface for the Npackd software package manager" TRUE)
option(NPACKD_BUILD_NPACKDG "Build the graphical interface for the Npackd software package manager" TRUE)
option(NPACKD_FORCE_STATIC_QT "Force generator to link Qt static" FALSE)
mark_as_advanced(NPACKD_FORCE_STATIC_QT)

find_package(QuaZip REQUIRED)
find_package(Qt5 COMPONENTS core xml sql REQUIRED)
if(${NPACKD_FORCE_STATIC_QT})
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

set(NPACKD_BASE_LIBS
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
  list(APPEND NPACKD_BASE_LIBS ${ZLIB_LIBRARIES})
endif()
if(${NPACKD_FORCE_STATIC_QT})
  list(APPEND NPACKD_BASE_LIBS
    qsqlite
    qtpcre2
  )
  add_definitions(-DQT_LINK_STATIC=1)
else()
  add_definitions(-DQT_LINK_STATIC=0)
endif()
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES CrystalIcons_LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

if(${NPACKD_BUILD_CLU})
  add_subdirectory(clu)
endif()
if(${NPACKD_BUILD_NCL})
  add_subdirectory(npackdcl)
endif()
if(${NPACKD_BUILD_NPACKDG})
  add_subdirectory(npackdg)
endif()
