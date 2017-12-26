# compiler warnings
if(CMAKE_COMPILER_IS_GNUCXX OR CMAKE_COMPILER_IS_CLANGXX OR CMAKE_COMPILER_IS_GNUCC)
  set(NPACKD_WARNING_FLAGS "-Wall -Winit-self -Wwrite-strings -Wextra -Wno-long-long -Wno-overloaded-virtual -Wno-missing-field-initializers -Wno-unused-parameter -Wno-unknown-pragmas")
  if(EMSCRIPTEN)
    set(NPACKD_WARNING_FLAGS "${NPACKD_WARNING_FLAGS} -Wno-warn-absolute-paths")
  elseif(NOT APPLE)
    set(NPACKD_WARNING_FLAGS "${NPACKD_WARNING_FLAGS} -Wno-unused-but-set-parameter")
  endif()
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