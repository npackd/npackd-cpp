cmake_minimum_required(VERSION 3.7 FATAL_ERROR)

project(npackdg CXX C)

set(CMAKE_ALLOW_LOOSE_LOOP_CONSTRUCTS ON)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH}
    "${CMAKE_CURRENT_SOURCE_DIR}/../cmake/")

if(WIN32)
    set(CMAKE_USE_RELATIVE_PATHS true)
    set(CMAKE_SUPPRESS_REGENERATION true)
endif()

#  -Wl,--major-os-version,6 -Wl,--minor-os-version,1
if(NOT MSVC)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map,${PROJECT_NAME}.map")

    if(${NPACKD_FORCE_STATIC})
        SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -static -static-libstdc++ -static-libgcc")
    endif()

    SET(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -g -Os -Wl,--subsystem,windows:6.1")
endif()

include(CheckCXXCompilerFlag)

include(../cmake/Common.cmake)

readVersion("../appveyor.yml")

#file(GLOB RESOURCE_IMAGES "src/*.png")
#file(COPY ${RESOURCE_IMAGES} src/npackdg.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

#set(wxWidgets_FIND_STYLE "win32")
#set(wxWidgets_CONFIGURATION mswu)
#find_package(wxWidgets REQUIRED)
#include(${wxWidgets_USE_FILE})

add_executable(npackdg "src/main.cpp")

#set_property(TARGET wxwidgets-cmake-example PROPERTY INTERPROCEDURAL_OPTIMIZATION TRUE)

target_include_directories(npackdg PRIVATE "C:/msys64/mingw64/include/wx-3.0")
target_include_directories(npackdg PRIVATE "C:/msys64/mingw64/lib/wx/include/msw-unicode-static-3.0")

target_link_libraries(npackdg
    wx_mswu_core-3.0
    wx_baseu-3.0
    comctl32
    uuid
    oleacc
    png
    z)

#target_link_libraries(wxwidgets-cmake-example ${wxWidgets_LIBRARIES})
