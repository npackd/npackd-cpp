# QUAZIP_FOUND               - QuaZip library was found
# QUAZIP_INCLUDE_DIRS        - Path to QuaZip include dir
# QUAZIP_LIBRARIES           - List of QuaZip libraries
# QUAZIP_DEFINE              - Compiler definitions for QuaZip
# QUAZIP_STATIC              - QuaZip is static

find_package(PkgConfig) 
pkg_check_modules(PC_QUAZIP QUIET QUAZIP) 

find_path(QUAZIP_INCLUDE_DIR
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

find_library(QUAZIP_LIBRARIES
    WIN32_DEBUG_POSTFIX d
    NAMES libquazip libquazip5 quazip5
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
    PATH_SUFFIXES QuaZip quazip QuaZip5 quazip5
)

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(QUAZIP FOUND_VAR QUAZIP_FOUND REQUIRED_VARS QUAZIP_LIBRARIES QUAZIP_INCLUDE_DIR)

set(QUAZIP_STATIC OFF)
if(QUAZIP_FOUND)
    foreach(_QZIP_LIB ${QUAZIP_LIBRARIES})
        get_filename_component(_QZIP_LIB_NAME ${_QZIP_LIB} NAME_WLE)
        if(${_QZIP_LIB_NAME} MATCHES "(.*)static[d]*")
            set(QUAZIP_STATIC ON)
        endif()
    endforeach()
endif()

if(QUAZIP_STATIC)
    set(QUAZIP_DEFINE "-DQUAZIP_STATIC")
    find_package(ZLIB REQUIRED)
else()
    set(QUAZIP_DEFINE "")
endif()

mark_as_advanced(QUAZIP_LIBRARIES QUAZIP_INCLUDE_DIR QUAZIP_STATIC QUAZIP_DEFINE)
