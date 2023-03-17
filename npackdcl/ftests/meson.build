project('ftests', 'c', 'cpp', default_options: ['c_std=c11', 'cpp_std=c++11'])

fs = import('fs') 
qt5_mod = import('qt5')
windows = import('windows')

deps =[
    dependency('quazip1-qt5'),
    dependency('qt5', modules: 'Sql'),
    dependency('qt5', modules: 'Xml'),
    dependency('qt5', modules: 'Test'),
    dependency('qt5', modules: 'Core')
]

CXX = meson.get_compiler('cpp')
foreach lib_name : ['userenv', 'winmm', 'ole32', 'uuid', 'wininet', 'psapi', 'version',
    'shlwapi', 'msi', 'netapi32', 'Ws2_32', 'taskschd'] 
    deps += CXX.find_library(lib_name)
endforeach

install_data('Rep.xml')

version = fs.read('../../appveyor.yml').strip().split('\n')[0].replace('version:', '').replace('.{build}', '').strip()
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

# TODO static builds
#  -static -static-libstdc++ -static-libgcc
#  qtpcre2 zstd z
#  -D QUAZIP_STATIC=1

add_project_link_arguments('-Wl,-Map,' + meson.project_name() + '.map', language: 'cpp')

processed = qt5_mod.preprocess(
    # Only headers that need moc should be put here
    moc_headers : ['src/app.h', '../../npackdg/src/job.h', 
    '../../npackdg/src/installedpackages.h', '../../npackdg/src/visiblejobs.h', 
    '../../npackdg/src/clprogress.h'] 
)

# TODO option(NPACKD_ADMIN "Force admin right on program" TRUE)
#if(NPACKD_ADMIN)
#  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=1)
#else()
#  target_compile_definitions(npackdcl PRIVATE -D NPACKD_ADMIN=0)
#endif()

src = ['src/app.cpp',
    '../../npackdg/src/downloader.cpp',
    '../../npackdg/src/commandline.cpp',
    '../../npackdg/src/repositoryxmlhandler.cpp',
    '../../npackdg/src/mysqlquery.cpp',
    '../../npackdg/src/wellknownprogramsthirdpartypm.cpp',
    '../../npackdg/src/abstractthirdpartypm.cpp',
    '../../npackdg/src/msithirdpartypm.cpp',
    '../../npackdg/src/installedpackagesthirdpartypm.cpp',
    '../../npackdg/src/controlpanelthirdpartypm.cpp',
    '../../npackdg/src/installoperation.cpp',
    '../../npackdg/src/dependency.cpp',
    '../../npackdg/src/packageversionfile.cpp',
    '../../npackdg/src/dbrepository.cpp',
    '../../npackdg/src/license.cpp',
    '../../npackdg/src/repository.cpp',
    '../../npackdg/src/job.cpp',
    '../../npackdg/src/hrtimer.cpp',
    '../../npackdg/src/package.cpp',
    '../../npackdg/src/installedpackageversion.cpp',
    '../../npackdg/src/abstractrepository.cpp',
    '../../npackdg/src/version.cpp',
    '../../npackdg/src/installedpackages.cpp',
    '../../npackdg/src/windowsregistry.cpp',
    '../../npackdg/src/packageversion.cpp',
    '../../npackdg/src/wpmutils.cpp',
    '../../npackdg/src/clprogress.cpp',
    '../../npackdg/src/packageutils.cpp',
    '../../npackdg/src/wuathirdpartypm.cpp',
    '../../npackdg/src/wuapi_i.c',
    '../../npackdg/src/comobject.cpp',
    '../../npackdg/src/dismthirdpartypm.cpp',
    '../../npackdg/src/sqlutils.cpp',
    '../../npackdg/src/dag.cpp',
    '../../npackdg/src/deptask.cpp',
    '../../npackdg/src/threadpool.cpp',
    '../../npackdg/src/lockedfiles.cpp',
    'src/main.cpp',
    processed]

incdir = include_directories('../../npackdg/src', 'src')    
executable(meson.project_name(), src, dependencies: deps, include_directories: incdir, 
    install: true)

