project('clu', 'c', 'cpp', default_options : ['c_std=c11', 'cpp_std=c++11'])

fs = import('fs') 
qt5_mod = import('qt5')

deps =[
    dependency('quazip1-qt5'),
    dependency('qt5', modules : 'Sql'),
    dependency('qt5', modules : 'Xml'),
    dependency('qt5', modules : 'Core')
]

CXX = meson.get_compiler('cpp')
foreach lib_name : ['userenv', 'winmm', 'ole32', 'uuid', 'wininet', 'psapi', 'version',
    'shlwapi', 'msi', 'netapi32', 'Ws2_32', 'taskschd'] 
    deps += CXX.find_library(lib_name)
endforeach

version = fs.read('../appveyor.yml').strip().split('\n')[0].replace('version:', '').replace('.{build}', '').strip()

#TODO NPACKD_VERSION_MAJOR, MINOR, PATCH, TWEAK

# -flto does not work (01.01.2020) probably because Qt is not compiled with LTO enabled
# -fno-exceptions -fno-unwind-tables do not reduce the binary size at all (05.01.2020)
add_global_arguments('-DUNICODE', '-D_UNICODE', '-DNOMINMAX',
    '-D_WIN32_WINNT=0x0601', # Windows 7
    '-DNPACKD_VERSION="' + version + '"', 
    '-Wall', '-Wwrite-strings', '-Wextra', '-Wno-unused-parameter', '-Wno-cast-function-type', '-Wduplicated-cond', 
    '-Wduplicated-branches', '-Wlogical-op',
    '-Wno-error=cast-qual', '-Wno-unused-local-typedefs', '-Wno-unused-variable',
    language : 'cpp')

# TODO static builds
#  -static -static-libstdc++ -static-libgcc
#  qtpcre2 zstd z
#  -D QUAZIP_STATIC=1

# TODO
# install(FILES ../LICENSE.txt DESTINATION ${CMAKE_INSTALL_PREFIX})


# TODO  -g -Os

# TODO
#configure_file(src/app.rc.in ${CMAKE_CURRENT_BINARY_DIR}/clu.rc @ONLY)
#file(COPY ../npackdg/src/app.ico DESTINATION ${CMAKE_CURRENT_BINARY_DIR})
#list(APPEND CLU_SOURCES ${CMAKE_CURRENT_BINARY_DIR}/clu.rc)


add_project_link_arguments('-Wl,-Map,clu.map', language: 'cpp')

processed = qt5_mod.preprocess(
    # Only headers that need moc should be put here
    moc_headers : ['../npackdg/src/job.h', '../npackdg/src/installedpackages.h', '../npackdg/src/clprogress.h'] 
)

src = ['src/main.cpp',
    'src/app.cpp',
    '../npackdg/src/job.cpp', 
    '../npackdg/src/installedpackages.cpp', 
    '../npackdg/src/clprogress.cpp',
    '../npackdg/src/package.cpp',
    '../npackdg/src/license.cpp',
    '../npackdg/src/dependency.cpp',
    '../npackdg/src/abstractrepository.cpp',
    '../npackdg/src/repository.cpp',
    '../npackdg/src/packageversion.cpp',
    '../npackdg/src/packageversionfile.cpp',
    '../npackdg/src/installedpackageversion.cpp',
    '../npackdg/src/windowsregistry.cpp',
    '../npackdg/src/abstractthirdpartypm.cpp',
    '../npackdg/src/controlpanelthirdpartypm.cpp',
    '../npackdg/src/commandline.cpp',
    '../npackdg/src/wpmutils.cpp',
    '../npackdg/src/hrtimer.cpp',
    '../npackdg/src/version.cpp',
    '../npackdg/src/installoperation.cpp',
    '../npackdg/src/dbrepository.cpp',
    '../npackdg/src/downloader.cpp',
    '../npackdg/src/repositoryxmlhandler.cpp',
    '../npackdg/src/installedpackagesthirdpartypm.cpp',
    '../npackdg/src/wellknownprogramsthirdpartypm.cpp',
    '../npackdg/src/msithirdpartypm.cpp',
    '../npackdg/src/mysqlquery.cpp',
    '../npackdg/src/packageutils.cpp',
    '../npackdg/src/wuathirdpartypm.cpp',
    '../npackdg/src/wuapi_i.c',
    '../npackdg/src/comobject.cpp',
    '../npackdg/src/sqlutils.cpp',
    '../npackdg/src/dag.cpp',
    '../npackdg/src/lockedfiles.cpp',
    '../npackdg/src/threadpool.cpp',
    '../npackdg/src/deptask.cpp',
    '../npackdcl/src/commandlinemessagehandler.cpp', processed]

incdir = include_directories('../npackdg/src', '../npackdcl/src')    
executable('clu', src, dependencies: deps, include_directories : incdir)
