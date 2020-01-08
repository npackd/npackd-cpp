set(FTESTS_SOURCES
    src/app.cpp
    ../../npackdg/src/downloader.cpp
    ../../npackdg/src/commandline.cpp
    ../../npackdg/src/repositoryxmlhandler.cpp
    ../../npackdg/src/mysqlquery.cpp
    ../../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../../npackdg/src/abstractthirdpartypm.cpp
    ../../npackdg/src/msithirdpartypm.cpp
    ../../npackdg/src/installedpackagesthirdpartypm.cpp
    ../../npackdg/src/controlpanelthirdpartypm.cpp
    ../../npackdg/src/installoperation.cpp
    ../../npackdg/src/dependency.cpp
    ../../npackdg/src/packageversionfile.cpp
    ../../npackdg/src/dbrepository.cpp
    ../../npackdg/src/license.cpp
    ../../npackdg/src/repository.cpp
    ../../npackdg/src/job.cpp
    ../../npackdg/src/hrtimer.cpp
    ../../npackdg/src/package.cpp
    ../../npackdg/src/installedpackageversion.cpp
    ../../npackdg/src/abstractrepository.cpp
    ../../npackdg/src/version.cpp
    ../../npackdg/src/installedpackages.cpp
    ../../npackdg/src/windowsregistry.cpp
    ../../npackdg/src/packageversion.cpp
    ../../npackdg/src/wpmutils.cpp
    ../../npackdg/src/clprogress.cpp
    ../../npackdg/src/urlinfo.cpp
    src/main.cpp
)
set(FTESTS_HEADERS
    src/app.h
    ../../npackdg/src/downloader.h
    ../../npackdg/src/commandline.h
    ../../npackdg/src/repositoryxmlhandler.h
    ../../npackdg/src/mysqlquery.h
    ../../npackdg/src/wellknownprogramsthirdpartypm.h
    ../../npackdg/src/abstractthirdpartypm.h
    ../../npackdg/src/msithirdpartypm.h
    ../../npackdg/src/installedpackagesthirdpartypm.h
    ../../npackdg/src/controlpanelthirdpartypm.h
    ../../npackdg/src/installoperation.h
    ../../npackdg/src/dependency.h
    ../../npackdg/src/packageversionfile.h
    ../../npackdg/src/dbrepository.h
    ../../npackdg/src/license.h
    ../../npackdg/src/repository.h
    ../../npackdg/src/job.h
    ../../npackdg/src/hrtimer.h
    ../../npackdg/src/package.h
    ../../npackdg/src/installedpackageversion.h
    ../../npackdg/src/abstractrepository.h
    ../../npackdg/src/version.h
    ../../npackdg/src/installedpackages.h
    ../../npackdg/src/windowsregistry.h
    ../../npackdg/src/packageversion.h
    ../../npackdg/src/wpmutils.h
    ../../npackdg/src/clprogress.h
    ../../npackdg/src/urlinfo.h
)

set(OUTPUT_FILE_NAME "ftests.exe")

if(MSVC)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ftests.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS test REQUIRED)

add_executable(ftests
	${FTESTS_SOURCES}
	${FTESTS_HEADERS}
)
target_link_libraries(ftests
    ${NPACKD_BASE_LIBS}
	Qt5::Test
)
if(${NPACKD_FORCE_STATIC_QT})
	target_link_libraries(ftests
		qtharfbuzz
		icuin
		icuuc
		icudt
		icutu
	)
endif()

install(TARGETS ftests DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32 AND NOT ${NPACKD_FORCE_STATIC_QT})
    install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime --pdb \"${CMAKE_INSTALL_PREFIX}/ftests.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")
endif()
