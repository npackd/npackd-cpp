include(CheckTypeSize)
CHECK_TYPE_SIZE("void*" OSMSCOUT_PTR_SIZE BUILTIN_TYPES_ONLY)
if(OSMSCOUT_PTR_SIZE EQUAL 8)
  set(NPACKD_PLATFORM_X64 TRUE)
  set(BITS 64)
else()
  set(NPACKD_PLATFORM_X64 FALSE)
  set(BITS 32)
endif()

# compiler warnings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC)
  set(NPACKD_WARNING_FLAGS "-Wall -Wwrite-strings -Wextra -Wno-unused-parameter -Wno-cast-function-type -Wduplicated-cond -Wduplicated-branches -Wlogical-op")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${NPACKD_WARNING_FLAGS}")
endif()

# Reads the version number from Version.txt
#
# txt: path to the Version.txt
macro(readVersion txt)
	file(STRINGS ${txt} READ_BUILD_NUMBER REGEX "version:.+" LIMIT_COUNT 1)

	message(STATUS "Read build number ${READ_BUILD_NUMBER}")

	string(REGEX MATCHALL "[0-9]+" _versionComponents "${READ_BUILD_NUMBER}")
	list(LENGTH _versionComponents _len)
	if (${_len} GREATER 0)
		list(GET _versionComponents 0 NPACKD_VERSION_MAJOR)
	else()
		set(NPACKD_VERSION_MAJOR 0)
	endif()
	if (${_len} GREATER 1)
		list(GET _versionComponents 1 NPACKD_VERSION_MINOR)
	else()
		set(NPACKD_VERSION_MINOR 0)
	endif()
	if (${_len} GREATER 2)
		list(GET _versionComponents 2 NPACKD_VERSION_PATCH)
	else()
		set(NPACKD_VERSION_PATCH 0)
	endif()
	if (${_len} GREATER 3)
		list(GET _versionComponents 3 NPACKD_VERSION_TWEAK)
	else()
		set(NPACKD_VERSION_TWEAK 0)
	endif()

	set(NPACKD_VERSION "${NPACKD_VERSION_MAJOR}.${NPACKD_VERSION_MINOR}.${NPACKD_VERSION_PATCH}.${NPACKD_VERSION_TWEAK}")

	message(STATUS "Build npackd v${NPACKD_VERSION}")
endmacro(readVersion)

if(MSVC)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /fp:fast /wd4251")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /Oi")
  if(CMAKE_CL_64)
    set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
    set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /bigobj")
  endif()
  if(MSVC_VERSION GREATER 1500 OR MSVC_VERSION EQUAL 1500)
    option(NPACKD_BUILD_MSVC_MP "Enable build with multiple processes in Visual Studio" TRUE)
  else()
    set(NPACKD_BUILD_MSVC_MP FALSE CACHE BOOL "Compiler option /MP requires at least Visual Studio 2008 (VS9) or newer" FORCE)
  endif()
  if(NPACKD_BUILD_MSVC_MP)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP")
  endif()
  add_definitions(-D_CRT_SECURE_NO_DEPRECATE -D_CRT_NONSTDC_NO_DEPRECATE -DDLL_EXPORT -DUNICODE -D_UNICODE)
endif()

# Windows 7
add_definitions(-D_WIN32_WINNT=0x0601)

# GCC compiler flags
if(MINGW)
  if(NOT NPACKD_PLATFORM_X64)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -march=i686")
  endif ()

  # -Wno-unused-variable is set to avoid many warnings in asyncdownloader.cpp
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-error=cast-qual -Wno-unused-local-typedefs -Wno-unused-variable")

  # -flto does not work (01.01.2020) probably because Qt is not compiled with LTO enabled

  #  -fno-exceptions -fno-unwind-tables do not reduce the binary size at all (05.01.2020)
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
endif()

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

# build should fail when compiler don't support standard defined by CMAKE_CXX_STANDARD
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_STANDARD 11)

add_definitions(-DUNICODE -D_UNICODE -DNOMINMAX)
