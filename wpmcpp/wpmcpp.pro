# -------------------------------------------------
# Project created by QtCreator 2010-06-20T15:15:32
# -------------------------------------------------

# Just a test:
# SYSTEM_CMD=$$(NPACKD_CL)/npackdcl.exe path --package "net.sourceforge.mingw-w64.MinGWW64"
# TST = $$system($${SYSTEM_CMD})
# message(This looks like ($$TST) to me)

QT += xml sql
TARGET = wpmcpp
TEMPLATE = app
SOURCES += main.cpp \
    mainwindow.cpp \
    packageversion.cpp \
    repository.cpp \
    job.cpp \
    downloader.cpp \
    wpmutils.cpp \
    package.cpp \
    packageversionfile.cpp \
    version.cpp \
    dependency.cpp \
    fileloader.cpp \
    fileloaderitem.cpp \
    installoperation.cpp \
    packageversionform.cpp \
    license.cpp \
    licenseform.cpp \
    windowsregistry.cpp \
    detectfile.cpp \
    uiutils.cpp \
    commandline.cpp \
    progressframe.cpp \
    messageframe.cpp \
    xmlutils.cpp \
    settingsframe.cpp \
    packageframe.cpp \
    selection.cpp \
    hrtimer.cpp \
    clprogress.cpp \
    mainframe.cpp \
    dbrepository.cpp \
    installedpackages.cpp \
    installedpackageversion.cpp \
    abstractrepository.cpp \
    packageitemmodel.cpp \
    abstractthirdpartypm.cpp \
    controlpanelthirdpartypm.cpp \
    msithirdpartypm.cpp \
    wellknownprogramsthirdpartypm.cpp \
    installedpackagesthirdpartypm.cpp \
    flowlayout.cpp \
    scandiskthirdpartypm.cpp
HEADERS += mainwindow.h \
    packageversion.h \
    repository.h \
    job.h \
    downloader.h \
    wpmutils.h \
    package.h \
    packageversionfile.h \
    version.h \
    dependency.h \
    fileloader.h \
    fileloaderitem.h \
    installoperation.h \
    packageversionform.h \
    license.h \
    licenseform.h \
    windowsregistry.h \
    detectfile.h \
    uiutils.h \
    commandline.h \
    progressframe.h \
    messageframe.h \
    xmlutils.h \
    settingsframe.h \
    mstask.h \
    packageframe.h \
    selection.h \
    hrtimer.h \
    clprogress.h \
    mainframe.h \
    dbrepository.h \
    installedpackages.h \
    installedpackageversion.h \
    abstractrepository.h \
    packageitemmodel.h \
    abstractthirdpartypm.h \
    controlpanelthirdpartypm.h \
    msithirdpartypm.h \
    wellknownprogramsthirdpartypm.h \
    installedpackagesthirdpartypm.h \
    flowlayout.h \
    scandiskthirdpartypm.h
FORMS += mainwindow.ui \
    packageversionform.ui \
    licenseform.ui \
    progressframe.ui \
    messageframe.ui \
    settingsframe.ui \
    packageframe.ui \
    mainframe.ui
TRANSLATIONS = wpmcpp_es.ts wpmcpp_ru.ts wpmcpp_fr.ts wpmcpp_de.ts
LIBS += -lquazip \
    -lole32 \
    -luuid \
    -lwininet \
    -lpsapi \
    -lshell32 \
    -lversion \
    -lshlwapi \
    -lmsi
CONFIG += embed_manifest_exe
CONFIG += static
RC_FILE = wpmcpp.rc
RESOURCES += wpmcpp.qrc
DEFINES+=QUAZIP_STATIC=1
INCLUDEPATH+=$$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
INCLUDEPATH+=$$(QUAZIP_PATH)/quazip
QMAKE_LIBDIR+=$$(QUAZIP_PATH)/quazip/release

# these 2 options can be used to add the debugging information to the "release"
# build
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_LFLAGS_RELEASE -= -Wl,-s

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror
QMAKE_LFLAGS += -static

QMAKE_LFLAGS_RELEASE += -Wl,-Map,wpmcpp_release.map

gprof {
    QMAKE_CXXFLAGS+=-pg
    QMAKE_LFLAGS+=-pg
}
