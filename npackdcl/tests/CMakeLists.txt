set(TESTS_SOURCES
    src/main.cpp
    ../../npackdg/src/visiblejobs.cpp
    ../../npackdg/src/repository.cpp
    ../../npackdg/src/version.cpp
    ../../npackdg/src/packageversionfile.cpp
    ../../npackdg/src/package.cpp
    ../../npackdg/src/packageversion.cpp
    ../../npackdg/src/job.cpp
    ../../npackdg/src/installoperation.cpp
    ../../npackdg/src/dependency.cpp
    ../../npackdg/src/wpmutils.cpp
    ../../npackdg/src/downloader.cpp
    ../../npackdg/src/license.cpp
    ../../npackdg/src/windowsregistry.cpp
    src/app.cpp
    ../../npackdg/src/commandline.cpp
    ../../npackdg/src/installedpackages.cpp
    ../../npackdg/src/installedpackageversion.cpp
    ../../npackdg/src/clprogress.cpp
    ../../npackdg/src/dbrepository.cpp
    ../../npackdg/src/abstractrepository.cpp
    ../../npackdg/src/abstractthirdpartypm.cpp
    ../../npackdg/src/msithirdpartypm.cpp
    ../../npackdg/src/controlpanelthirdpartypm.cpp
    ../../npackdg/src/wellknownprogramsthirdpartypm.cpp
    ../../npackdg/src/hrtimer.cpp
    ../../npackdg/src/repositoryxmlhandler.cpp
    ../../npackdg/src/mysqlquery.cpp
    ../../npackdg/src/installedpackagesthirdpartypm.cpp
    ../../npackdg/src/urlinfo.cpp
    )
set(TESTS_HEADERS
    ../../npackdg/src/visiblejobs.h
    ../../npackdg/src/repository.h
    ../../npackdg/src/version.h
    ../../npackdg/src/packageversionfile.h
    ../../npackdg/src/package.h
    ../../npackdg/src/packageversion.h
    ../../npackdg/src/job.h
    ../../npackdg/src/installoperation.h
    ../../npackdg/src/dependency.h
    ../../npackdg/src/wpmutils.h
    ../../npackdg/src/downloader.h
    ../../npackdg/src/license.h
    ../../npackdg/src/windowsregistry.h
    src/app.h
    ../../npackdg/src/installedpackages.h
    ../../npackdg/src/installedpackageversion.h
    ../../npackdg/src/commandline.h
    ../../npackdg/src/clprogress.h
    ../../npackdg/src/dbrepository.h
    ../../npackdg/src/abstractrepository.h
    ../../npackdg/src/abstractthirdpartypm.h
    ../../npackdg/src/msithirdpartypm.h
    ../../npackdg/src/controlpanelthirdpartypm.h
    ../../npackdg/src/wellknownprogramsthirdpartypm.h
    ../../npackdg/src/hrtimer.h
    ../../npackdg/src/repositoryxmlhandler.h
    ../../npackdg/src/mysqlquery.h
    ../../npackdg/src/installedpackagesthirdpartypm.h
    ../../npackdg/src/urlinfo.h
)

if(MSVC)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/tests.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS test REQUIRED)

add_executable(tests
    ${TESTS_SOURCES}
    ${TESTS_HEADERS}
)
target_link_libraries(tests
    ${NPACKED_BASE_LIBS}
    Qt5::Test
)
if(${NPACKED_FORCE_STATIC_QT})
    target_link_libraries(tests
        qtharfbuzz
        icuin
        icuuc
        icudt
        icutu
    )
endif()

install(TARGETS tests DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32 AND NOT ${NPACKED_FORCE_STATIC_QT})
  install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime --pdb \"${CMAKE_INSTALL_PREFIX}/tests.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")
endif()
