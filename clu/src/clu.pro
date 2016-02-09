#-------------------------------------------------
#
# Project created by QtCreator 2011-09-17T18:36:18
#
#-------------------------------------------------

QT += core xml sql
QT       -= gui

TARGET = clu
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
    -lmsi \
    -lnetapi32

SOURCES += main.cpp \
    app.cpp \
    ../../wpmcpp/src/windowsregistry.cpp \
    ../../wpmcpp/src/commandline.cpp \
    ../../wpmcpp/src/wpmutils.cpp \
    ../../wpmcpp/src/job.cpp \
    ../../wpmcpp/src/hrtimer.cpp \
    ../../wpmcpp/src/version.cpp

HEADERS += \
    app.h \
    ../../wpmcpp/src/windowsregistry.h \
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
    -Wno-missing-field-initializers -Wno-unused-parameter -fno-exceptions


