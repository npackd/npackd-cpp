# QUAZIP_FOUND               - QuaZip library was found
# QUAZIP_INCLUDE_DIRS        - Path to QuaZip include dir
# QUAZIP_LIBRARIES           - List of QuaZip libraries

set(_OLD_FIND_SUFFIXES ${CMAKE_FIND_LIBRARY_SUFFIXES})
set(CMAKE_FIND_LIBRARY_SUFFIXES ".lib" ".a" ".so" ".sl" ".dylib" ".dll.a")
FIND_PACKAGE(ZLIB REQUIRED)
#find_package(${ARGN})
set(CMAKE_FIND_LIBRARY_SUFFIXES ${_OLD_FIND_SUFFIXES})
unset(_OLD_FIND_SUFFIXES)

FIND_PACKAGE(PkgConfig) 
PKG_CHECK_MODULES(PC_QUAZIP QUIET QUAZIP) 

FIND_PATH(QUAZIP_INCLUDE_DIRS
    NAMES quazip.h
    HINTS ${QUAZIP_ROOT}/include
			${PC_QUAZIP_INCLUDEDIR}
          ${PC_QUAZIP_INCLUDE_DIRS}
          $ENV{QUAZIP_HOME}/quazip
          $ENV{QUAZIP_HOME}/include
          $ENV{QUAZIP_ROOT}/include
          /usr/local/include
          /usr/include
          /quazip/include
		  "C:/Programme/"
		  "C:/Program Files"
    PATH_SUFFIXES QuaZip quazip QuaZip5 quazip5
)

FIND_LIBRARY(QUAZIP_LIBRARIES
	WIN32_DEBUG_POSTFIX d
    NAMES libquazip libquazip5
    HINTS ${QUAZIP_ROOT}/lib
			${PC_QUAZIP_LIBDIR}
          ${PC_QUAZIP_LIBRARY_DIRS}
          $ENV{QUAZIP_HOME}/quazip/release
          $ENV{QUAZIP_HOME}/lib
          $ENV{QUAZIP_ROOT}/lib
          /usr/local/lib
          /usr/lib
          /lib
          /quazip/lib
    PATH_SUFFIXES agg agg2
)

INCLUDE(FindPackageHandleStandardArgs)
FIND_PACKAGE_HANDLE_STANDARD_ARGS(QuaZip DEFAULT_MSG QUAZIP_INCLUDE_DIRS QUAZIP_LIBRARIES)
