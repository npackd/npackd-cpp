NPACKD_VERSION = $$system(type ..\\wpmcpp\\version.txt)
DEFINES += NPACKD_VERSION=\\\"$$NPACKD_VERSION\\\"

QT += xml sql
QT -= gui
TARGET = npackdcl
CONFIG += console
CONFIG -= app_bundle
TEMPLATE = app
RC_FILE = npackdcl.rc
LIBS += -lquazip \
    -lole32 \
    -luuid \
    -lwininet \
    -lpsapi \
    -lversion \
    -lshlwapi \
    -lmsi
SOURCES += main.cpp \
    ../wpmcpp/repository.cpp \
    ../wpmcpp/version.cpp \
    ../wpmcpp/packageversionfile.cpp \
    ../wpmcpp/package.cpp \
    ../wpmcpp/packageversion.cpp \
    ../wpmcpp/job.cpp \
    ../wpmcpp/installoperation.cpp \
    ../wpmcpp/dependency.cpp \
    ../wpmcpp/wpmutils.cpp \
    ../wpmcpp/downloader.cpp \
    ../wpmcpp/license.cpp \
    ../wpmcpp/windowsregistry.cpp \
    ../wpmcpp/detectfile.cpp \
    app.cpp \
    ../wpmcpp/commandline.cpp \
    ../wpmcpp/xmlutils.cpp \
    ../wpmcpp/installedpackages.cpp \
    ../wpmcpp/installedpackageversion.cpp \
    ../wpmcpp/clprogress.cpp \
    ../wpmcpp/dbrepository.cpp \
    ../wpmcpp/abstractrepository.cpp \
    ../wpmcpp/abstractthirdpartypm.cpp \
    ../wpmcpp/msithirdpartypm.cpp \
    ../wpmcpp/controlpanelthirdpartypm.cpp \
    ../wpmcpp/wellknownprogramsthirdpartypm.cpp \
    ../wpmcpp/hrtimer.cpp \
    ../wpmcpp/repositoryxmlhandler.cpp \
    ../wpmcpp/mysqlquery.cpp \
    ../wpmcpp/installedpackagesthirdpartypm.cpp \
    ../wpmcpp/cbsthirdpartypm.cpp
HEADERS += ../wpmcpp/repository.h \
    ../wpmcpp/version.h \
    ../wpmcpp/packageversionfile.h \
    ../wpmcpp/package.h \
    ../wpmcpp/packageversion.h \
    ../wpmcpp/job.h \
    ../wpmcpp/installoperation.h \
    ../wpmcpp/dependency.h \
    ../wpmcpp/wpmutils.h \
    ../wpmcpp/downloader.h \
    ../wpmcpp/license.h \
    ../wpmcpp/windowsregistry.h \
    ../wpmcpp/detectfile.h \
    app.h \
    ../wpmcpp/installedpackages.h \
    ../wpmcpp/installedpackageversion.h \
    ../wpmcpp/commandline.h \
    ../wpmcpp/xmlutils.h \
    ../wpmcpp/clprogress.h \
    ../wpmcpp/dbrepository.h \
    ../wpmcpp/abstractrepository.h \
    ../wpmcpp/abstractthirdpartypm.h \
    ../wpmcpp/msithirdpartypm.h \
    ../wpmcpp/controlpanelthirdpartypm.h \
    ../wpmcpp/wellknownprogramsthirdpartypm.h \
    ../wpmcpp/hrtimer.h \
    ../wpmcpp/repositoryxmlhandler.h \
    ../wpmcpp/mysqlquery.h \
    ../wpmcpp/installedpackagesthirdpartypm.h \
    ../wpmcpp/cbsthirdpartypm.h
FORMS += 

CONFIG += static

DEFINES+=QUAZIP_STATIC=1

INCLUDEPATH+=$$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
INCLUDEPATH+=$$(QUAZIP_PATH)/quazip

QMAKE_LIBDIR+=$$(QUAZIP_PATH)/quazip/release

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror
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
