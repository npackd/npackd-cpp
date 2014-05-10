NPACKD_VERSION = $$system(type ..\\..\\wpmcpp\\version.txt)
DEFINES += NPACKD_VERSION=\\\"$$NPACKD_VERSION\\\"

QT += xml sql
TARGET = npackdcl
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
RC_FILE = npackdcl.rc
LIBS += -lquazip \
    -lz \
    -lole32 \
    -luuid \
    -lwininet \
    -lpsapi \
    -lversion \
    -lshlwapi \
    -lmsi
SOURCES += main.cpp \
    ../../wpmcpp/src/visiblejobs.cpp \
    ../../wpmcpp/src/installthread.cpp \
    ../../wpmcpp/src/updaterepositorythread.cpp \
    ../../wpmcpp/src/repository.cpp \
    ../../wpmcpp/src/version.cpp \
    ../../wpmcpp/src/packageversionfile.cpp \
    ../../wpmcpp/src/package.cpp \
    ../../wpmcpp/src/packageversion.cpp \
    ../../wpmcpp/src/job.cpp \
    ../../wpmcpp/src/installoperation.cpp \
    ../../wpmcpp/src/dependency.cpp \
    ../../wpmcpp/src/wpmutils.cpp \
    ../../wpmcpp/src/downloader.cpp \
    ../../wpmcpp/src/license.cpp \
    ../../wpmcpp/src/windowsregistry.cpp \
    ../../wpmcpp/src/detectfile.cpp \
    app.cpp \
    ../../wpmcpp/src/commandline.cpp \
    ../../wpmcpp/src/xmlutils.cpp \
    ../../wpmcpp/src/installedpackages.cpp \
    ../../wpmcpp/src/installedpackageversion.cpp \
    ../../wpmcpp/src/clprogress.cpp \
    ../../wpmcpp/src/dbrepository.cpp \
    ../../wpmcpp/src/abstractrepository.cpp \
    ../../wpmcpp/src/abstractthirdpartypm.cpp \
    ../../wpmcpp/src/msithirdpartypm.cpp \
    ../../wpmcpp/src/controlpanelthirdpartypm.cpp \
    ../../wpmcpp/src/wellknownprogramsthirdpartypm.cpp \
    ../../wpmcpp/src/hrtimer.cpp \
    ../../wpmcpp/src/repositoryxmlhandler.cpp \
    ../../wpmcpp/src/mysqlquery.cpp \
    ../../wpmcpp/src/installedpackagesthirdpartypm.cpp \
    ../../wpmcpp/src/cbsthirdpartypm.cpp
HEADERS += ../../wpmcpp/src/installthread.h \
    ../../wpmcpp/src/visiblejobs.h \
    ../../wpmcpp/src/updaterepositorythread.h \
    ../../wpmcpp/src/repository.h \
    ../../wpmcpp/src/version.h \
    ../../wpmcpp/src/packageversionfile.h \
    ../../wpmcpp/src/package.h \
    ../../wpmcpp/src/packageversion.h \
    ../../wpmcpp/src/job.h \
    ../../wpmcpp/src/installoperation.h \
    ../../wpmcpp/src/dependency.h \
    ../../wpmcpp/src/wpmutils.h \
    ../../wpmcpp/src/downloader.h \
    ../../wpmcpp/src/license.h \
    ../../wpmcpp/src/windowsregistry.h \
    ../../wpmcpp/src/detectfile.h \
    app.h \
    ../../wpmcpp/src/installedpackages.h \
    ../../wpmcpp/src/installedpackageversion.h \
    ../../wpmcpp/src/commandline.h \
    ../../wpmcpp/src/xmlutils.h \
    ../../wpmcpp/src/clprogress.h \
    ../../wpmcpp/src/dbrepository.h \
    ../../wpmcpp/src/abstractrepository.h \
    ../../wpmcpp/src/abstractthirdpartypm.h \
    ../../wpmcpp/src/msithirdpartypm.h \
    ../../wpmcpp/src/controlpanelthirdpartypm.h \
    ../../wpmcpp/src/wellknownprogramsthirdpartypm.h \
    ../../wpmcpp/src/hrtimer.h \
    ../../wpmcpp/src/repositoryxmlhandler.h \
    ../../wpmcpp/src/mysqlquery.h \
    ../../wpmcpp/src/installedpackagesthirdpartypm.h \
    ../../wpmcpp/src/cbsthirdpartypm.h
FORMS += 

CONFIG += static

DEFINES+=QUAZIP_STATIC=1

INCLUDEPATH+=$$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
INCLUDEPATH+=$$(QUAZIP_PATH)/quazip
INCLUDEPATH+=../../wpmcpp/src/

QMAKE_LIBDIR+=$$(QUAZIP_PATH)/quazip/release

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror \
    -Wno-missing-field-initializers -Wno-unused-parameter
QMAKE_LFLAGS += -static

QMAKE_LFLAGS_RELEASE += -Wl,-Map,npackdcl_release.map

# these 2 options can be used to add the debugging information to the "release"
# build
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_LFLAGS_RELEASE -= -Wl,-s

gprof {
    QMAKE_CXXFLAGS+=-pg
    QMAKE_LFLAGS+=-pg
}

test {
    DEFINES += TEST
}
