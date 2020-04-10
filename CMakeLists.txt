cmake_minimum_required(VERSION 3.7 FATAL_ERROR)
project(npackd-cpp)

add_subdirectory(npackdg)
add_subdirectory(npackdcl)
add_subdirectory(clu)

option(NPACKD_FORCE_STATIC "Force generator to link statically" TRUE)
mark_as_advanced(NPACKD_FORCE_STATIC)
