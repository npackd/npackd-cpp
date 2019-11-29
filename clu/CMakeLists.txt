set(CLU_SOURCES
    src/main.cpp
    src/app.cpp
    ../npackdg/src/clprogress.cpp
    ../npackdg/src/package.cpp
    ../npackdg/src/license.cpp
    ../npackdg/src/dependency.cpp
    ../npackdg/src/abstractrepository.cpp
    ../npackdg/src/repository.cpp
    ../npackdg/src/packageversion.cpp
    ../npackdg/src/packageversionfile.cpp
    ../npackdg/src/installedpackageversion.cpp
    ../npackdg/src/windowsregistry.cpp
    ../npackdg/src/abstractthirdpartypm.cpp
    ../npackdg/src/controlpanelthirdpartypm.cpp
    ../npackdg/src/commandline.cpp
    ../npackdg/src/wpmutils.cpp
    ../npackdg/src/job.cpp
    ../npackdg/src/hrtimer.cpp
    ../npackdg/src/version.cpp
    ../npackdg/src/installedpackages.cpp
    ../npackdg/src/installoperation.cpp
    ../npackdg/src/dbrepository.cpp
    ../npackdg/src/downloader.cpp
    ../npackdg/src/repositoryxmlhandler.cpp
    ../npackdg/src/installedpackagesthirdpartypm.cpp
    ../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../npackdg/src/msithirdpartypm.cpp
    ../npackdg/src/mysqlquery.cpp
    ../npackdg/src/urlinfo.cpp
        )
set(CLU_HEADERS
    src/app.h
    ../npackdg/src/clprogress.h
    ../npackdg/src/package.h
    ../npackdg/src/license.h
    ../npackdg/src/dependency.h
    ../npackdg/src/abstractrepository.h
    ../npackdg/src/repository.h
    ../npackdg/src/packageversion.h
    ../npackdg/src/packageversionfile.h
    ../npackdg/src/installedpackageversion.h
    ../npackdg/src/windowsregistry.h
    ../npackdg/src/abstractthirdpartypm.h
    ../npackdg/src/controlpanelthirdpartypm.h
    ../npackdg/src/commandline.h
    ../npackdg/src/wpmutils.h
    ../npackdg/src/job.h
    ../npackdg/src/hrtimer.h
    ../npackdg/src/version.h
    ../npackdg/src/installedpackages.h
    ../npackdg/src/installoperation.h
    ../npackdg/src/dbrepository.h
    ../npackdg/src/downloader.h
    ../npackdg/src/repositoryxmlhandler.h
    ../npackdg/src/installedpackagesthirdpartypm.h
    ../npackdg/src/wellknownprogramsthirdpartypm.h
    ../npackdg/src/msithirdpartypm.h
    ../npackdg/src/mysqlquery.h
    ../npackdg/src/urlinfo.h
)

configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/clu.rc @ONLY)
file(COPY ../npackdg/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND CLU_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/clu.rc)
if(MSVC)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/clu.vcxproj.user @ONLY)
endif() 

add_executable(clu
    ${CLU_SOURCES}
    ${CLU_HEADERS}
)

target_link_libraries(clu ${NPACKED_BASE_LIBS})

install(TARGETS clu DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32)
  install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime \"${CMAKE_INSTALL_PREFIX}/clu.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\" COMMAND_ECHO STDOUT)")
endif()
