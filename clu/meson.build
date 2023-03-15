project('clu', 'c', 'cpp')

qt5_mod = import('qt5')

deps =[
    dependency('quazip1-qt5'),
    dependency('qt5', modules : 'Sql'),
    dependency('qt5', modules : 'Xml'),
    dependency('qt5', modules : 'Core')
]

CXX = meson.get_compiler('cpp')
libs_you_need_to_link = ['userenv', 'winmm', 'ole32', 'uuid', 'wininet', 'psapi', 'version',
    'shlwapi', 'msi', 'netapi32', 'Ws2_32', 'taskschd']
foreach lib_name : libs_you_need_to_link 
    deps += CXX.find_library(lib_name, dirs : ['C:\\msys64\\mingw64\\lib'])
endforeach

add_global_arguments('-std=c++11', '-DUNICODE', '-D_UNICODE',  '-DNPACKD_VERSION="1.27.0.0"', 
    '-Wall', '-Wwrite-strings', '-Wextra', '-Wno-unused-parameter', '-Wno-cast-function-type', '-Wduplicated-cond', 
    '-Wduplicated-branches', '-Wlogical-op',
    language : 'cpp')

processed = qt5_mod.preprocess(
    # Only headers that need moc should be put here
  moc_headers : ['../npackdg/src/job.h', '../npackdg/src/installedpackages.h', '../npackdg/src/clprogress.h'] 
  # must have #include"moc_helperFile.cpp" 
  #moc_sources : []
)

dir = '../npackdg/src'
src = ['src/main.cpp',
    'src/app.cpp',
    '../npackdg/src/job.cpp', '../npackdg/src/installedpackages.cpp', '../npackdg/src/clprogress.cpp',
    dir / 'package.cpp',
    dir / 'license.cpp',
    dir / 'dependency.cpp',
    dir / 'abstractrepository.cpp',
    dir / 'repository.cpp',
    dir / 'packageversion.cpp',
    dir / 'packageversionfile.cpp',
    dir / 'installedpackageversion.cpp',
    dir / 'windowsregistry.cpp',
    dir / 'abstractthirdpartypm.cpp',
    dir / 'controlpanelthirdpartypm.cpp',
    dir / 'commandline.cpp',
    dir / 'wpmutils.cpp',
    dir / 'hrtimer.cpp',
    dir / 'version.cpp',
    dir / 'installoperation.cpp',
    dir / 'dbrepository.cpp',
    dir / 'downloader.cpp',
    dir / 'repositoryxmlhandler.cpp',
    dir / 'installedpackagesthirdpartypm.cpp',
    dir / 'wellknownprogramsthirdpartypm.cpp',
    dir / 'msithirdpartypm.cpp',
    dir / 'mysqlquery.cpp',
    dir / 'packageutils.cpp',
    dir / 'wuathirdpartypm.cpp',
    dir / 'wuapi_i.c',
    dir / 'comobject.cpp',
    dir / 'sqlutils.cpp',
    dir / 'dag.cpp',
    dir / 'lockedfiles.cpp',
    dir / 'threadpool.cpp',
    dir / 'deptask.cpp',
    'C:\\Users\\User\\Documents\\npackd-cpp\\npackdcl\\src' / 'commandlinemessagehandler.cpp', processed]
incdir = include_directories('../npackdg/src', '../npackdcl/src')    
executable('clu', src, dependencies: deps, include_directories : incdir)
