cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(exeproxy C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON) 
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/../cmake/")
SET(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} -mconsole -static -municode")

if(WIN32)
  set(CMAKE_USE_RELATIVE_PATHS true)
  set(CMAKE_SUPPRESS_REGENERATION true)
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

SET(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -static-libgcc" )

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

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

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(EXEPROXY_SOURCES
    src/main.c
)
set(EXEPROXY_HEADERS
)

set(OUTPUT_FILE_NAME "exeproxy.exe")

add_executable(exeproxy ${EXEPROXY_SOURCES})

target_link_libraries(exeproxy)
target_include_directories(exeproxy PRIVATE ${CMAKE_SOURCE_DIR}/../wpmcpp/src)
target_compile_definitions(exeproxy PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}")

install(TARGETS exeproxy DESTINATION ${CMAKE_INSTALL_PREFIX})
install(FILES LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})

# todo
# contains(QT_ARCH, i386) {
#     QMAKE_CXXFLAGS_RELEASE += -flto
# }

# todo
# QMAKE_CXXFLAGS += -static-libgcc -Werror \
#     -Wno-missing-field-initializers -Wno-unused-parameter -municode
# 
# QMAKE_LFLAGS_RELEASE += -Wl,-Map,exeproxy_release.map
