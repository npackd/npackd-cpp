set(NPACKDCL_SOURCES
    ../npackdg/src/visiblejobs.cpp
    ../npackdg/src/repository.cpp
    ../npackdg/src/version.cpp
    ../npackdg/src/packageversionfile.cpp
    ../npackdg/src/package.cpp
    ../npackdg/src/packageversion.cpp
    ../npackdg/src/job.cpp
    ../npackdg/src/installoperation.cpp
    ../npackdg/src/dependency.cpp
    ../npackdg/src/wpmutils.cpp
    ../npackdg/src/downloader.cpp
    ../npackdg/src/license.cpp
    ../npackdg/src/windowsregistry.cpp
    ../npackdg/src/commandline.cpp
    ../npackdg/src/installedpackages.cpp
    ../npackdg/src/installedpackageversion.cpp
    ../npackdg/src/clprogress.cpp
    ../npackdg/src/dbrepository.cpp
    ../npackdg/src/abstractrepository.cpp
    ../npackdg/src/abstractthirdpartypm.cpp
    ../npackdg/src/msithirdpartypm.cpp
    ../npackdg/src/controlpanelthirdpartypm.cpp
    ../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../npackdg/src/hrtimer.cpp
    ../npackdg/src/repositoryxmlhandler.cpp
    ../npackdg/src/mysqlquery.cpp
    ../npackdg/src/installedpackagesthirdpartypm.cpp
    ../npackdg/src/urlinfo.cpp
    src/commandlinemessagehandler.cpp
    src/main.cpp
    src/app.cpp
)
set(NPACKDCL_HEADERS
    ../npackdg/src/visiblejobs.h
    ../npackdg/src/repository.h
    ../npackdg/src/version.h
    ../npackdg/src/packageversionfile.h
    ../npackdg/src/package.h
    ../npackdg/src/packageversion.h
    ../npackdg/src/job.h
    ../npackdg/src/installoperation.h
    ../npackdg/src/dependency.h
    ../npackdg/src/wpmutils.h
    ../npackdg/src/downloader.h
    ../npackdg/src/license.h
    ../npackdg/src/windowsregistry.h
    ../npackdg/src/installedpackages.h
    ../npackdg/src/installedpackageversion.h
    ../npackdg/src/commandline.h
    ../npackdg/src/clprogress.h
    ../npackdg/src/dbrepository.h
    ../npackdg/src/abstractrepository.h
    ../npackdg/src/abstractthirdpartypm.h
    ../npackdg/src/msithirdpartypm.h
    ../npackdg/src/controlpanelthirdpartypm.h
    ../npackdg/src/wellknownprogramsthirdpartypm.h
    ../npackdg/src/hrtimer.h
    ../npackdg/src/repositoryxmlhandler.h
    ../npackdg/src/mysqlquery.h
    ../npackdg/src/installedpackagesthirdpartypm.h
    ../npackdg/src/urlinfo.h
    src/commandlinemessagehandler.h
    src/app.h
)

list(APPEND NPACKDCL_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
if(MSVC)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ncl.vcxproj.user @ONLY)
endif() 

add_executable(ncl
    ${NPACKDCL_SOURCES}
    ${NPACKDCL_HEADERS}
)
generateRC(ncl OFF)

target_link_libraries(ncl ${NPACKD_BASE_LIBS})
if(${NPACKD_FORCE_STATIC_QT})
	target_link_libraries(ncl
		mingwex
		icuin
		icuuc
		icudt
		icutu
	)
endif()

install(TARGETS ncl DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32 AND NOT ${NPACKD_FORCE_STATIC_QT})
	install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime --pdb \"${CMAKE_INSTALL_PREFIX}/ncl.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")
endif()

if(${NPACKD_BUILD_TESTS})
	add_subdirectory(tests)
	add_subdirectory(ftests)
endif()