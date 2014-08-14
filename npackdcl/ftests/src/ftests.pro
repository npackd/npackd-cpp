NPACKD_VERSION = $$system(type ..\\..\\..\\wpmcpp\\src\\version.txt)
DEFINES += NPACKD_VERSION=\\\"$$NPACKD_VERSION\\\"

QT       += core xml sql

QT       -= gui

TARGET = ftests
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

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
    app.cpp \
    ..\..\..\wpmcpp\src\detectfile.cpp \
    ..\..\..\wpmcpp\src\downloader.cpp \
    ..\..\..\wpmcpp\src\commandline.cpp \
    ..\..\..\wpmcpp\src\repositoryxmlhandler.cpp \
    ..\..\..\wpmcpp\src\mysqlquery.cpp \
    ..\..\..\wpmcpp\src\wellknownprogramsthirdpartypm.cpp \
    ..\..\..\wpmcpp\src\abstractthirdpartypm.cpp \
    ..\..\..\wpmcpp\src\msithirdpartypm.cpp \
    ..\..\..\wpmcpp\src\installedpackagesthirdpartypm.cpp \
    ..\..\..\wpmcpp\src\controlpanelthirdpartypm.cpp \
    ..\..\..\wpmcpp\src\installoperation.cpp \
    ..\..\..\wpmcpp\src\dependency.cpp \
    ..\..\..\wpmcpp\src\packageversionfile.cpp \
    ..\..\..\wpmcpp\src\dbrepository.cpp \
    ..\..\..\wpmcpp\src\license.cpp \
    ..\..\..\wpmcpp\src\xmlutils.cpp \
    ..\..\..\wpmcpp\src\repository.cpp \
    ..\..\..\wpmcpp\src\job.cpp \
    ..\..\..\wpmcpp\src\hrtimer.cpp \
    ..\..\..\wpmcpp\src\package.cpp \
    ..\..\..\wpmcpp\src\installedpackageversion.cpp \
    ..\..\..\wpmcpp\src\abstractrepository.cpp \
    ..\..\..\wpmcpp\src\version.cpp \
    ..\..\..\wpmcpp\src\installedpackages.cpp \
    ..\..\..\wpmcpp\src\windowsregistry.cpp \
    ..\..\..\wpmcpp\src\packageversion.cpp \
    ..\..\..\wpmcpp\src\wpmutils.cpp \
    ..\..\..\wpmcpp\src\clprogress.cpp

HEADERS += \
    app.h \
    ..\..\..\wpmcpp\src\detectfile.h \
    ..\..\..\wpmcpp\src\downloader.h \
    ..\..\..\wpmcpp\src\commandline.h \
    ..\..\..\wpmcpp\src\repositoryxmlhandler.h \
    ..\..\..\wpmcpp\src\mysqlquery.h \
    ..\..\..\wpmcpp\src\wellknownprogramsthirdpartypm.h \
    ..\..\..\wpmcpp\src\abstractthirdpartypm.h \
    ..\..\..\wpmcpp\src\msithirdpartypm.h \
    ..\..\..\wpmcpp\src\installedpackagesthirdpartypm.h \
    ..\..\..\wpmcpp\src\controlpanelthirdpartypm.h \
    ..\..\..\wpmcpp\src\installoperation.h \
    ..\..\..\wpmcpp\src\dependency.h \
    ..\..\..\wpmcpp\src\packageversionfile.h \
    ..\..\..\wpmcpp\src\dbrepository.h \
    ..\..\..\wpmcpp\src\license.h \
    ..\..\..\wpmcpp\src\xmlutils.h \
    ..\..\..\wpmcpp\src\repository.h \
    ..\..\..\wpmcpp\src\job.h \
    ..\..\..\wpmcpp\src\hrtimer.h \
    ..\..\..\wpmcpp\src\package.h \
    ..\..\..\wpmcpp\src\installedpackageversion.h \
    ..\..\..\wpmcpp\src\abstractrepository.h \
    ..\..\..\wpmcpp\src\version.h \
    ..\..\..\wpmcpp\src\installedpackages.h \
    ..\..\..\wpmcpp\src\windowsregistry.h \
    ..\..\..\wpmcpp\src\packageversion.h \
    ..\..\..\wpmcpp\src\wpmutils.h \
    ..\..\..\wpmcpp\src\clprogress.h

CONFIG += static

DEFINES+=QUAZIP_STATIC=1

INCLUDEPATH+=$$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
INCLUDEPATH+=$$(QUAZIP_PATH)/quazip
INCLUDEPATH+=../../../wpmcpp/src/

QMAKE_LIBDIR+=$$(QUAZIP_PATH)/quazip/release

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror \
    -Wno-missing-field-initializers -Wno-unused-parameter

QMAKE_LFLAGS += -static

QMAKE_LFLAGS_RELEASE += -Wl,-Map,ftests_release.map

# these 2 options can be used to add the debugging information to the "release"
# build
QMAKE_CXXFLAGS_RELEASE += -g
QMAKE_LFLAGS_RELEASE -= -Wl,-s
