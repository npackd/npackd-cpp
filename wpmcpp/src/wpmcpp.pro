NPACKD_VERSION = $$system(type ..\\version.txt)
DEFINES += NPACKD_VERSION=\\\"$$NPACKD_VERSION\\\"

QT += xml sql widgets winextras
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
    scandiskthirdpartypm.cpp \
    mysqlquery.cpp \
    repositoryxmlhandler.cpp \
    cbsthirdpartypm.cpp \
    installthread.cpp \
    updaterepositorythread.cpp \
    scanharddrivesthread.cpp \
    visiblejobs.cpp \
    clprocessor.cpp \
    progresstree2.cpp \
    progressframe.cpp
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
    scandiskthirdpartypm.h \
    mysqlquery.h \
    repositoryxmlhandler.h \
    cbsthirdpartypm.h \
    msoav2.h \
    installthread.h \
    updaterepositorythread.h \
    scanharddrivesthread.h \
    visiblejobs.h \
    clprocessor.h \
    progresstree2.h \
    progressframe.h
FORMS += mainwindow.ui \
    packageversionform.ui \
    licenseform.ui \
    messageframe.ui \
    settingsframe.ui \
    packageframe.ui \
    mainframe.ui \
    progressframe.ui
TRANSLATIONS = wpmcpp_es.ts wpmcpp_ru.ts wpmcpp_fr.ts wpmcpp_de.ts
LIBS += -lquazip \
    -lz \
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
CONFIG += qt
RC_FILE = wpmcpp.rc
RESOURCES += wpmcpp.qrc
DEFINES += QUAZIP_STATIC=1
INCLUDEPATH += $$(QUAZIP_PATH)/quazip
QMAKE_LIBDIR += $$(QUAZIP_PATH)/quazip/release

# these 2 options can be used to add the debugging information to the "release"
# build
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_LFLAGS_RELEASE -= -Wl,-s

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror \
    -Wno-missing-field-initializers -Wno-unused-parameter
QMAKE_LFLAGS += -static

QMAKE_LFLAGS_RELEASE += -Wl,-Map,wpmcpp_release.map

gprof {
    QMAKE_CXXFLAGS_RELEASE -= -O2
    QMAKE_CXXFLAGS_RELEASE += -O1
    QMAKE_CXXFLAGS+=-pg
    QMAKE_LFLAGS+=-pg
}
