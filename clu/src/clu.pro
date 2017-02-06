#-------------------------------------------------
#
# Project created by QtCreator 2011-09-17T18:36:18
#
#-------------------------------------------------

QT += core xml sql
QT       -= gui
NPACKD_VERSION = $$system(type ..\\..\\wpmcpp\version.txt)
DEFINES += NPACKD_VERSION=\\\"$$NPACKD_VERSION\\\"

TARGET = clu
CONFIG   += console
CONFIG   -= app_bundle
CONFIG += c++11
CONFIG += static


TEMPLATE = app

LIBS += -lquazip \
    -lz \
    -lole32 \
    -luuid \
    -lwininet \
    -lpsapi \
    -lversion \
    -lshlwapi \
    -lmsi \
    -lnetapi32

SOURCES += main.cpp \
    app.cpp \
    ../../wpmcpp/src/clprogress.cpp \
    ../../wpmcpp/src/package.cpp \
    ../../wpmcpp/src/license.cpp \
    ../../wpmcpp/src/dependency.cpp \
    ../../wpmcpp/src/detectfile.cpp \
    ../../wpmcpp/src/abstractrepository.cpp \
    ../../wpmcpp/src/repository.cpp \
    ../../wpmcpp/src/packageversion.cpp \
    ../../wpmcpp/src/packageversionfile.cpp \
    ../../wpmcpp/src/installedpackageversion.cpp \
    ../../wpmcpp/src/windowsregistry.cpp \
    ../../wpmcpp/src/abstractthirdpartypm.cpp \
    ../../wpmcpp/src/controlpanelthirdpartypm.cpp \
    ../../wpmcpp/src/commandline.cpp \
    ../../wpmcpp/src/wpmutils.cpp \
    ../../wpmcpp/src/job.cpp \
    ../../wpmcpp/src/hrtimer.cpp \
    ../../wpmcpp/src/version.cpp

HEADERS += \
    app.h \
    ../../wpmcpp/src/clprogress.h \
    ../../wpmcpp/src/package.h \
    ../../wpmcpp/src/license.h \
    ../../wpmcpp/src/dependency.h \
    ../../wpmcpp/src/detectfile.h \
    ../../wpmcpp/src/abstractrepository.h \
    ../../wpmcpp/src/repository.h \
    ../../wpmcpp/src/packageversion.h \
    ../../wpmcpp/src/packageversionfile.h \
    ../../wpmcpp/src/installedpackageversion.h \
    ../../wpmcpp/src/windowsregistry.h \
    ../../wpmcpp/src/abstractthirdpartypm.h \
    ../../wpmcpp/src/controlpanelthirdpartypm.h \
    ../../wpmcpp/src/commandline.h \
    ../../wpmcpp/src/wpmutils.h \
    ../../wpmcpp/src/job.h \
    ../../wpmcpp/src/hrtimer.h \
    ../../wpmcpp/src/version.h

DEFINES+=QUAZIP_STATIC=1

INCLUDEPATH+=$$[QT_INSTALL_PREFIX]/src/3rdparty/zlib
INCLUDEPATH+=$$(QUAZIP_PATH)/quazip
INCLUDEPATH+=../../wpmcpp/src/

QMAKE_LIBDIR+=$$(QUAZIP_PATH)/quazip/release

QMAKE_CXXFLAGS += -static-libstdc++ -static-libgcc -Werror \
    -Wno-missing-field-initializers -Wno-unused-parameter
QMAKE_CXXFLAGS -= -fexceptions
QMAKE_CXXFLAGS_RELEASE -= -O2
QMAKE_CXXFLAGS_RELEASE += -Os -g
contains(QT_ARCH, i386) {
    QMAKE_CXXFLAGS_RELEASE += -flto
}

QMAKE_LFLAGS += -static
QMAKE_LFLAGS_RELEASE -= -Wl,-s -Os
QMAKE_LFLAGS_RELEASE += -Wl,-Map,clu_release.map
contains(QT_ARCH, i386) {
    QMAKE_LFLAGS_RELEASE += -flto
}

