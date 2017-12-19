project(ncl CXX)

set(NPACKDCL_SOURCES
    ../wpmcpp/src/visiblejobs.cpp
    ../wpmcpp/src/repository.cpp
    ../wpmcpp/src/version.cpp
    ../wpmcpp/src/packageversionfile.cpp
    ../wpmcpp/src/package.cpp
    ../wpmcpp/src/packageversion.cpp
    ../wpmcpp/src/job.cpp
    ../wpmcpp/src/installoperation.cpp
    ../wpmcpp/src/dependency.cpp
    ../wpmcpp/src/wpmutils.cpp
    ../wpmcpp/src/downloader.cpp
    ../wpmcpp/src/license.cpp
    ../wpmcpp/src/windowsregistry.cpp
    ../wpmcpp/src/detectfile.cpp
    ../wpmcpp/src/commandline.cpp
    ../wpmcpp/src/installedpackages.cpp
    ../wpmcpp/src/installedpackageversion.cpp
    ../wpmcpp/src/clprogress.cpp
    ../wpmcpp/src/dbrepository.cpp
    ../wpmcpp/src/abstractrepository.cpp
    ../wpmcpp/src/abstractthirdpartypm.cpp
    ../wpmcpp/src/msithirdpartypm.cpp
    ../wpmcpp/src/controlpanelthirdpartypm.cpp
    ../wpmcpp/src/wellknownprogramsthirdpartypm.cpp
    ../wpmcpp/src/hrtimer.cpp
    ../wpmcpp/src/repositoryxmlhandler.cpp
    ../wpmcpp/src/mysqlquery.cpp
    ../wpmcpp/src/installedpackagesthirdpartypm.cpp
	src/main.cpp
    src/app.cpp
)
set(NPACKDCL_HEADERS
	../wpmcpp/src/visiblejobs.h
    ../wpmcpp/src/repository.h
    ../wpmcpp/src/version.h
    ../wpmcpp/src/packageversionfile.h
    ../wpmcpp/src/package.h
    ../wpmcpp/src/packageversion.h
    ../wpmcpp/src/job.h
    ../wpmcpp/src/installoperation.h
    ../wpmcpp/src/dependency.h
    ../wpmcpp/src/wpmutils.h
    ../wpmcpp/src/downloader.h
    ../wpmcpp/src/license.h
    ../wpmcpp/src/windowsregistry.h
    ../wpmcpp/src/detectfile.h
    ../wpmcpp/src/installedpackages.h
    ../wpmcpp/src/installedpackageversion.h
    ../wpmcpp/src/commandline.h
    ../wpmcpp/src/clprogress.h
    ../wpmcpp/src/dbrepository.h
    ../wpmcpp/src/abstractrepository.h
    ../wpmcpp/src/abstractthirdpartypm.h
    ../wpmcpp/src/msithirdpartypm.h
    ../wpmcpp/src/controlpanelthirdpartypm.h
    ../wpmcpp/src/wellknownprogramsthirdpartypm.h
    ../wpmcpp/src/hrtimer.h
    ../wpmcpp/src/repositoryxmlhandler.h
    ../wpmcpp/src/mysqlquery.h
    ../wpmcpp/src/installedpackagesthirdpartypm.h
    src/app.h
    src/stable.h
)
if(MSVC)
	set(OUTPUT_FILE_NAME "ncl.exe")
	configure_file(${CMAKE_SOURCE_DIR}/cmake/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/npackdcl.rc @ONLY)
	file(COPY ../wpmcpp/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
	list(APPEND NPACKDCL_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/npackdcl.rc)

	# Configure the template file that allows debugging
	set(QT_USE_IMPORTED_TARGETS TRUE)
	find_package(Qt5Core REQUIRED)
	set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/ncl.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql REQUIRED)
add_executable(ncl
	${NPACKDCL_SOURCES}
	${NPACKDCL_HEADERS}
)
#qt5_add_translation(WPMCPP_TRANSLATIONS_QM ${WPMCPP_TRANSLATIONS})
qt5_use_modules(ncl Core Xml Sql)
target_link_libraries(ncl
	${QUAZIP_LIBRARIES}
	${ZLIB_LIBRARIES}
	qtpcre
    ole32
    uuid
    wininet
    psapi
    version
    shlwapi
    msi
    netapi32
	Ws2_32
)
target_include_directories(ncl PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS} ${CMAKE_SOURCE_DIR}/wpmcpp/src)
target_compile_definitions(ncl PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS ncl DESTINATION ${CMAKE_INSTALL_PREFIX})
if(MSVC)
	set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb)
	set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll)
	install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
