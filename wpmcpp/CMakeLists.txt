project(npackd CXX)

file(GLOB RESOURCE_IMAGES "src/*.png")
file(COPY ${RESOURCE_IMAGES} src/wpmcpp.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(WPMCPP_SOURCES
	src/main.cpp
	src/mainwindow.cpp
	src/packageversion.cpp
	src/repository.cpp
	src/job.cpp
	src/downloader.cpp
	src/wpmutils.cpp
	src/package.cpp
	src/packageversionfile.cpp
	src/version.cpp
	src/dependency.cpp
	src/fileloader.cpp
	src/installoperation.cpp
	src/packageversionform.cpp
	src/license.cpp
	src/licenseform.cpp
	src/windowsregistry.cpp
	src/detectfile.cpp
	src/uiutils.cpp
	src/commandline.cpp
	src/messageframe.cpp
	src/settingsframe.cpp
	src/packageframe.cpp
	src/selection.cpp
	src/hrtimer.cpp
	src/clprogress.cpp
	src/mainframe.cpp
	src/dbrepository.cpp
	src/installedpackages.cpp
	src/installedpackageversion.cpp
	src/abstractrepository.cpp
	src/packageitemmodel.cpp
	src/abstractthirdpartypm.cpp
	src/controlpanelthirdpartypm.cpp
	src/msithirdpartypm.cpp
	src/wellknownprogramsthirdpartypm.cpp
	src/installedpackagesthirdpartypm.cpp
	src/flowlayout.cpp
	src/scandiskthirdpartypm.cpp
	src/mysqlquery.cpp
	src/repositoryxmlhandler.cpp
	src/cbsthirdpartypm.cpp
	src/visiblejobs.cpp
	src/progresstree2.cpp
	src/downloadsizefinder.cpp
	src/clprocessor.cpp
	src/exportrepositoryframe.cpp
	src/wpmcpp_plugin_import.cpp
	${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.qrc
)
set(WPMCPP_HEADERS
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
	src/detectfile.h
	src/uiutils.h
	src/commandline.h
	src/messageframe.h
	src/settingsframe.h
	src/mstask.h
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
	src/scandiskthirdpartypm.h
	src/mysqlquery.h
	src/repositoryxmlhandler.h
	src/cbsthirdpartypm.h
	src/msoav2.h
	src/visiblejobs.h
	src/clprocessor.h
	src/progresstree2.h
	src/downloadsizefinder.h
	src/concurrent.h
	src/stable.h
	src/exportrepositoryframe.h
)
set(WPMCPP_FORMS
	src/mainwindow.ui
	src/packageversionform.ui
	src/licenseform.ui
	src/messageframe.ui
	src/settingsframe.ui
	src/packageframe.ui
	src/mainframe.ui
	src/exportrepositoryframe.ui
)
set(WPMCPP_TRANSLATIONS
	src/wpmcpp_es.ts
	src/wpmcpp_ru.ts
	src/wpmcpp_fr.ts
	src/wpmcpp_de.ts
)

set(OUTPUT_FILE_NAME "npackd.exe")
configure_file(${CMAKE_SOURCE_DIR}/cmake/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.rc @ONLY)
file(COPY src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
list(APPEND WPMCPP_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/wpmcpp.rc)

if(MSVC)
	# Configure the template file that allows debugging
	set(QT_USE_IMPORTED_TARGETS TRUE)
	find_package(Qt5Core REQUIRED)
	set(QT_BIN_DIR ${_qt5Core_install_prefix}/bin)
	configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/npackd.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS xml sql widgets winextras LinguistTools REQUIRED)
add_executable(npackd WIN32
	${WPMCPP_SOURCES}
	${WPMCPP_HEADERS}
	${WPMCPP_FORMS}
)
qt5_add_translation(WPMCPP_TRANSLATIONS_QM ${WPMCPP_TRANSLATIONS})
qt5_use_modules(npackd Gui Core Xml Sql Widgets WinExtras)
target_link_libraries(npackd
	${QUAZIP_LIBRARIES}
	${ZLIB_LIBRARIES}
	
	qwindows
	Qt5PlatformSupport 
	qtfreetype
	
	qdds 
	qicns 
	qico 
	qjp2 
	qmng 
	qtga 
	qtiff 
	qwbmp 
	qwebp 
	
	imm32
	winmm 
	glu32 
	mpr
	
	qtharfbuzzng
	qtpcre
	opengl32
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
target_include_directories(npackd PRIVATE ${QUAZIP_INCLUDE_DIRS} ${ZLIB_INCLUDE_DIRS})
target_compile_definitions(npackd PRIVATE -D NPACKD_VERSION="${NPACKD_VERSION}" -D QUAZIP_STATIC=1)

install(TARGETS npackd DESTINATION ${CMAKE_INSTALL_PREFIX})
if(MSVC)
	set(QT5_BIN_DEBUG ${_qt5Core_install_prefix}/bin/Qt5Cored.dll ${_qt5Core_install_prefix}/bin/Qt5Cored.pdb ${_qt5Core_install_prefix}/bin/Qt5Xmld.dll ${_qt5Core_install_prefix}/bin/Qt5Xmld.pdb ${_qt5Core_install_prefix}/bin/Qt5Sqld.dll ${_qt5Core_install_prefix}/bin/Qt5Sqld.pdb ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.dll ${_qt5Core_install_prefix}/bin/Qt5Widgetsd.pdb ${_qt5Core_install_prefix}/bin/Qt5Guid.dll ${_qt5Core_install_prefix}/bin/Qt5Guid.pdb)
	set(QT5_BIN_RELEASE ${_qt5Core_install_prefix}/bin/Qt5Core.dll ${_qt5Core_install_prefix}/bin/Qt5Xml.dll ${_qt5Core_install_prefix}/bin/Qt5Sql.dll ${_qt5Core_install_prefix}/bin/Qt5Widgets.dll ${_qt5Core_install_prefix}/bin/Qt5Gui.dll)
	install(FILES ${QT5_BIN_DEBUG} CONFIGURATIONS Debug DESTINATION ${CMAKE_INSTALL_PREFIX})
	install(FILES ${QT5_BIN_RELEASE} CONFIGURATIONS Release DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
install(FILES CrystalIcons_LICENSE.txt LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})
