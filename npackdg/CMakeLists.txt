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

set(NPACKDG_SOURCES
    src/main.cpp
#    src/mainwindow.cpp
#    src/packageversion.cpp
#    src/repository.cpp
#    src/job.cpp
#    src/downloader.cpp
#    src/wpmutils.cpp
#    src/package.cpp
#    src/packageversionfile.cpp
#    src/version.cpp
#    src/dependency.cpp
#    src/fileloader.cpp
#    src/installoperation.cpp
#    src/packageversionform.cpp
#    src/license.cpp
#    src/licenseform.cpp
#    src/windowsregistry.cpp
#    src/uiutils.cpp
#    src/commandline.cpp
#    src/messageframe.cpp
#    src/settingsframe.cpp
#    src/packageframe.cpp
#    src/selection.cpp
#    src/hrtimer.cpp
#    src/clprogress.cpp
#    src/mainframe.cpp
#    src/dbrepository.cpp
#    src/installedpackages.cpp
#    src/installedpackageversion.cpp
#    src/abstractrepository.cpp
#    src/packageitemmodel.cpp
#    src/abstractthirdpartypm.cpp
#    src/controlpanelthirdpartypm.cpp
#    src/msithirdpartypm.cpp
#    src/wellknownprogramsthirdpartypm.cpp
#    src/installedpackagesthirdpartypm.cpp
#    src/flowlayout.cpp
#    src/mysqlquery.cpp
#    src/repositoryxmlhandler.cpp
#    src/visiblejobs.cpp
#    src/progresstree2.cpp
#    src/downloadsizefinder.cpp
#    src/clprocessor.cpp
#    src/exportrepositoryframe.cpp
#    src/npackdg_plugin_import.cpp
#    src/urlinfo.cpp
#    src/asyncdownloader.cpp
#    src/uimessagehandler.cpp
#    src/packageutils.cpp
#    src/wuathirdpartypm.cpp
#    src/wuapi_i.c
#    src/comobject.cpp
#    src/repositoriesitemmodel.cpp
#    ${CMAKE_CURRENT_BINARY_DIR}/npackdg.qrc
)
set(NPACKDG_HEADERS
    src/mainwindow.h
    src/packageversion.h
    src/repository.h
    src/job.h
    src/downloader.h
    src/wpmutils.h
    src/package.h
    src/packageversionfile.h
    src/version.h
    src/dependency.h
    src/fileloader.h
    src/installoperation.h
    src/packageversionform.h
    src/license.h
    src/licenseform.h
    src/windowsregistry.h
    src/uiutils.h
    src/commandline.h
    src/messageframe.h
    src/settingsframe.h
    src/packageframe.h
    src/selection.h
    src/hrtimer.h
    src/clprogress.h
    src/mainframe.h
    src/dbrepository.h
    src/installedpackages.h
    src/installedpackageversion.h
    src/abstractrepository.h
    src/packageitemmodel.h
    src/abstractthirdpartypm.h
    src/controlpanelthirdpartypm.h
    src/msithirdpartypm.h
    src/wellknownprogramsthirdpartypm.h
    src/installedpackagesthirdpartypm.h
    src/flowlayout.h
    src/mysqlquery.h
    src/repositoryxmlhandler.h
    src/msoav2.h
    src/visiblejobs.h
    src/clprocessor.h
    src/progresstree2.h
    src/downloadsizefinder.h
    src/concurrent.h
    src/stable.h
    src/exportrepositoryframe.h
    src/urlinfo.h
    src/asyncdownloader.h
    src/uimessagehandler.h
    src/packageutils.h
    src/wuathirdpartypm.h
    src/wuapi.h
    src/comobject.h
    src/repositoriesitemmodel.h
)
set(NPACKDG_FORMS
    src/mainwindow.ui
    src/packageversionform.ui
    src/licenseform.ui
    src/messageframe.ui
    src/settingsframe.ui
    src/packageframe.ui
    src/mainframe.ui
    src/exportrepositoryframe.ui
)
set(NPACKDG_TRANSLATIONS
    src/npackdg_es.ts
    src/npackdg_ru.ts
    src/npackdg_fr.ts
    src/npackdg_de.ts
)

file(COPY src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
if(${NPACKD_ADMIN})
    file(COPY src/npackdg.manifest DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
    configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc @ONLY)
else()
    configure_file(../npackdcl/src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc @ONLY)
endif()
list(APPEND NPACKDG_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/npackdg.rc)

#set(wxWidgets_FIND_STYLE "win32")
#set(wxWidgets_CONFIGURATION mswu)
#find_package(wxWidgets REQUIRED)
#include(${wxWidgets_USE_FILE})

add_executable(npackdg WIN32
    ${NPACKDG_SOURCES}
#    ${NPACKDG_HEADERS}
#    ${NPACKDG_FORMS}
)

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
