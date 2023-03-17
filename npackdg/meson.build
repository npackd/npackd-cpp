project('npackdg', 'c', 'cpp', default_options: ['c_std=c11', 'cpp_std=c++11'])

fs = import('fs') 
qt5_mod = import('qt5')
windows = import('windows')

deps =[
    dependency('quazip1-qt5'),
    dependency('qt5', modules: 'Gui'),
    dependency('qt5', modules: 'Svg'),
    dependency('qt5', modules: 'Widgets'),
    dependency('qt5', modules: 'WinExtras'),
# TODO    dependency('qt5', modules: 'LinguistTools'),
# TODO    dependency('qt5', modules: 'FontDatabaseSupport'),
# TODO    dependency('qt5', modules: 'QSvgIconPlugin'),
# TODO    dependency('qt5', modules: 'QSvgPlugin'),
    dependency('qt5', modules: 'Sql'),
    dependency('qt5', modules: 'Xml'),
    dependency('qt5', modules: 'Core')
]

CXX = meson.get_compiler('cpp')
foreach lib_name : ['userenv', 'winmm', 'ole32', 'uuid', 'wininet', 'psapi', 'version',
    'shlwapi', 'msi', 'netapi32', 'Ws2_32', 'taskschd'] 
    deps += CXX.find_library(lib_name)
endforeach

version = fs.read('../appveyor.yml').strip().split('\n')[0].replace('version:', '').replace('.{build}', '').strip()
version_parts = version.split('.')

# -flto does not work (01.01.2020) probably because Qt is not compiled with LTO enabled
# -fno-exceptions -fno-unwind-tables do not reduce the binary size at all (05.01.2020)
add_global_arguments('-DUNICODE', '-D_UNICODE', '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0601', # Windows 7
    '-DNPACKD_VERSION="' + version + '"', 
    '-Wall', '-Wwrite-strings', '-Wextra', '-Wno-unused-parameter', '-Wno-cast-function-type', '-Wduplicated-cond', 
    '-Wduplicated-branches', '-Wlogical-op',
    '-Wno-error=cast-qual', '-Wno-unused-local-typedefs', '-Wno-unused-variable', '-g',
    language: 'cpp')

# TODO
# file(GLOB RESOURCE_IMAGES "src/*.png")
# file(COPY ${RESOURCE_IMAGES} src/npackdg.qrc DESTINATION ${CMAKE_CURRENT_BINARY_DIR})


# TODO static builds
#  -static -static-libstdc++ -static-libgcc
#  qtpcre2 zstd z
#  -D QUAZIP_STATIC=1

# TODO
# if(NPACKD_FORCE_STATIC)
#     SET(NPACKDG_LIBRARIES ${NPACKDG_LIBRARIES} qsqlite qicns qico qjpeg qgif qtga qtiff qwbmp qwebp qwindows qwindowsvistastyle)
#     SET(NPACKDG_LIBRARIES ${NPACKDG_LIBRARIES} mingwex Qt5ThemeSupport Qt5EventDispatcherSupport Qt5FontDatabaseSupport Qt5PlatformCompositorSupport Qt5WindowsUIAutomationSupport qdirect2d)
#     SET(NPACKDG_LIBRARIES ${NPACKDG_LIBRARIES} jasper icuin icuuc icudt icutu qtpcre2 qtharfbuzz qtfreetype qtlibpng jpeg zstd z)
# endif()

install_data('../LICENSE.txt')
install_data('../CrystalIcons_LICENSE.txt')

# TODO
# file(TO_CMAKE_PATH "$ENV{EXEPROXY}" EXEPROXY)
# install(FILES ${EXEPROXY}/exeproxy.exe DESTINATION ${CMAKE_INSTALL_PREFIX})
# if(WIN32 AND NOT NPACKD_FORCE_STATIC)
#     install(CODE "execute_process(COMMAND \"${WINDEPLOYQT_EXECUTABLE}\" --compiler-runtime --pdb \"${CMAKE_INSTALL_PREFIX}/npackdg.exe\" WORKING_DIRECTORY \"${_qt_bin_dir}\")")
# endif()

conf = configuration_data()
conf.set('NPACKD_VERSION_MAJOR', version_parts.length() > 0 ? version_parts[0] : '0')
conf.set('NPACKD_VERSION_MINOR', version_parts.length() > 1 ? version_parts[1] : '0')
conf.set('NPACKD_VERSION_PATCH', version_parts.length() > 2 ? version_parts[2] : '0')
conf.set('NPACKD_VERSION_TWEAK', version_parts.length() > 3 ? version_parts[3] : '0')
conf.set('OUTPUT_FILE_NAME', meson.project_name() + '.exe')
configure_file(input: 'src/app.rc.in',
  output: meson.project_name() + '.rc',
  configuration: conf)
configure_file(input: 'src/app.ico',
  output: 'app.ico', copy: true)
rc = windows.compile_resources(meson.current_build_dir() / meson.project_name() + '.rc')

foreach f : ['app.png',
    'update.png',
    'wait.png',
    'uninstall.png',
    'fileissue.png',
    'gotosite.png',
    'install.png',
    'broken.png',
    'add.png'] 
    configure_file(input: 'src' / f,
    output: f, copy: true)
endforeach

add_project_link_arguments('-Wl,-Map,' + meson.project_name() + '.map', language: 'cpp')

qms = qt5_mod.compile_translations(ts_files: 
    ['src/npackdg_ru.ts',
    'src/npackdg_de.ts',
    'src/npackdg_es.ts',
    'src/npackdg_fr.ts'])

configure_file(input: 'src' / meson.project_name() + '.qrc',
  output: meson.project_name() + '.qrc', copy: true)

processed = qt5_mod.preprocess(
    # Only headers that need moc should be put here
    moc_headers : ['src/job.h', 
        'src/installedpackages.h', 'src/visiblejobs.h', 
        'src/clprogress.h',
        'src/mainwindow.h',
        'src/fileloader.h',
        'src/packageframe.h', 'src/exportrepositoryframe.h', 
        'src/licenseform.h', 'src/mainframe.h', 
        'src/messageframe.h', 'src/packageversionform.h',
        'src/settingsframe.h',
        'src/progresstree2.h',
        'src/downloader.h',
        'src/asyncdownloader.h'],
    ui_files: ['src/packageframe.ui', 'src/exportrepositoryframe.ui', 
        'src/licenseform.ui', 'src/mainframe.ui', 'src/mainwindow.ui',
        'src/messageframe.ui', 'src/packageversionform.ui',
        'src/settingsframe.ui'],
    qresources: meson.current_build_dir() / meson.project_name() + '.qrc', 
)


# TODO option(NPACKD_ADMIN "Force admin right on program" TRUE)
#if(NPACKD_ADMIN)
#  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=1)
#else()
#  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=0)
#endif()

# TODO /npackdg.qrc

# TODO
# QT5_CREATE_TRANSLATION(qm_files src/ ${NPACKDG_TRANSLATIONS} OPTIONS -locations none)
# qt5_add_translation(NPACKDG_TRANSLATIONS_QM ${NPACKDG_TRANSLATIONS})

src = ['src/main.cpp',
    'src/mainwindow.cpp',
    'src/packageversion.cpp',
    'src/repository.cpp',
    'src/job.cpp',
    'src/downloader.cpp',
    'src/wpmutils.cpp',
    'src/package.cpp',
    'src/packageversionfile.cpp',
    'src/version.cpp',
    'src/dependency.cpp',
    'src/fileloader.cpp',
    'src/installoperation.cpp',
    'src/packageversionform.cpp',
    'src/license.cpp',
    'src/licenseform.cpp',
    'src/windowsregistry.cpp',
    'src/uiutils.cpp',
    'src/commandline.cpp',
    'src/messageframe.cpp',
    'src/settingsframe.cpp',
    'src/packageframe.cpp',
    'src/selection.cpp',
    'src/hrtimer.cpp',
    'src/clprogress.cpp',
    'src/mainframe.cpp',
    'src/dbrepository.cpp',
    'src/installedpackages.cpp',
    'src/installedpackageversion.cpp',
    'src/abstractrepository.cpp',
    'src/packageitemmodel.cpp',
    'src/abstractthirdpartypm.cpp',
    'src/controlpanelthirdpartypm.cpp',
    'src/msithirdpartypm.cpp',
    'src/wellknownprogramsthirdpartypm.cpp',
    'src/installedpackagesthirdpartypm.cpp',
    'src/flowlayout.cpp',
    'src/mysqlquery.cpp',
    'src/repositoryxmlhandler.cpp',
    'src/visiblejobs.cpp',
    'src/progresstree2.cpp',
    'src/clprocessor.cpp',
    'src/exportrepositoryframe.cpp',
    'src/asyncdownloader.cpp',
    'src/uimessagehandler.cpp',
    'src/packageutils.cpp',
    'src/wuathirdpartypm.cpp',
    'src/wuapi_i.c',
    'src/comobject.cpp',
    'src/repositoriesitemmodel.cpp',
    'src/dismthirdpartypm.cpp',
    'src/sqlutils.cpp',
    'src/threadpool.cpp',
    'src/dag.cpp',
    'src/deptask.cpp',
    'src/lockedfiles.cpp', 
    qms, processed, rc]

incdir = include_directories('src')    
executable(meson.project_name(), src, dependencies: deps, include_directories: incdir, 
    install: true)

