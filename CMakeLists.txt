cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(npackd-cpp)

set(BUILD_SHARED_LIBS NO)

add_subdirectory(npackdg)
add_subdirectory(npackdcl)
add_subdirectory(clu)
add_subdirectory(quazip)

option(NPACKD_FORCE_STATIC "Force generator to link statically" OFF)
mark_as_advanced(NPACKD_FORCE_STATIC)
