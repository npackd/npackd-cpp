file(GLOB RESOURCE_IMAGES "src/*.png")
file(COPY ${RESOURCE_IMAGES} src/npackdg.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})

set(NPACKDG_SOURCES
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
    src/mysqlquery.cpp
    src/repositoryxmlhandler.cpp
    src/visiblejobs.cpp
    src/progresstree2.cpp
    src/downloadsizefinder.cpp
    src/clprocessor.cpp
    src/exportrepositoryframe.cpp
    src/npackdg_plugin_import.cpp
    src/urlinfo.cpp
    src/asyncdownloader.cpp
    src/uimessagehandler.cpp
    ${CMAKE_CURRENT_BINARY_DIR}/npackdg.qrc
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

list(APPEND NPACKDG_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/app.rc)
if(MSVC)
    configure_file(${CMAKE_SOURCE_DIR}/cmake/UserTemplate.user.in ${CMAKE_CURRENT_BINARY_DIR}/npackdg.vcxproj.user @ONLY)
endif() 

find_package(Qt5 COMPONENTS Gui xml sql widgets WinExtras LinguistTools REQUIRED)

add_executable(npackdg WIN32
    ${NPACKDG_SOURCES}
    ${NPACKDG_HEADERS}
    ${NPACKDG_FORMS}
)
generateRC(npackdg ${NPACKD_ADMIN})
QT5_CREATE_TRANSLATION(qm_files src/ ${NPACKDG_TRANSLATIONS} OPTIONS -locations none)
qt5_add_translation(NPACKDG_TRANSLATIONS_QM ${NPACKDG_TRANSLATIONS})

target_link_libraries(npackdg
    ${NPACKED_BASE_LIBS}
	Qt5::Gui
	Qt5::WinExtras
	Qt5::Widgets
    imm32
    glu32 
    mpr
    wtsapi32
    opengl32
    UxTheme
    Dwmapi
)
if(${NPACKED_FORCE_STATIC_QT})
    target_link_libraries(npackdg
		Qt5VulkanSupport
		Qt5ThemeSupport
		Qt5FontDatabaseSupport
		Qt5EventDispatcherSupport
		Qt5PlatformCompositorSupport
		Qt5WindowsUIAutomationSupport
		qdirect2d
		qwindows
		qwindowsvistastyle
        mingwex
        jasper
        icuin
        icuuc
        icudt
        icutu
        qtpcre2
        qtharfbuzz
        qtfreetype
        qtlibpng
        jpeg
        qicns
        qico
        qjpeg
        qgif
        qtga
        qtiff
        qwbmp
        qwebp
    )
endif()

install(TARGETS npackdg DESTINATION ${CMAKE_INSTALL_PREFIX})
if(WIN32 AND NOT ${NPACKED_FORCE_STATIC_QT})
  install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --no-compiler-runtime --pdb \"${CMAKE_INSTALL_PREFIX}/npackdg.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")
endif()

file(TO_CMAKE_PATH "$ENV{EXEPROXY}" EXEPROXY)
if(EXISTS "${EXEPROXY}/exeproxy.exe")
	install(FILES ${EXEPROXY}/exeproxy.exe DESTINATION ${CMAKE_INSTALL_PREFIX})
endif()
