#-------------------------------------------------
#
# Project created by QtCreator 2011-09-17T18:36:18
#
#-------------------------------------------------

QT       += core

QT       -= gui

TARGET = clu
CONFIG   += console
CONFIG   -= app_bundle

TEMPLATE = app

LIBS += -lole32 \
    -luuid \
    -lwininet \
    -lpsapi \
    -lversion \
    -lshlwapi \
    -lmsi

SOURCES += main.cpp \
    app.cpp \
    ../wpmcpp/windowsregistry.cpp \
    ../wpmcpp/commandline.cpp \
    ../wpmcpp/wpmutils.cpp \
    ../wpmcpp/job.cpp \
    ../wpmcpp/version.cpp

HEADERS += \
    app.h \
    ../wpmcpp/windowsregistry.h \
    ../wpmcpp/commandline.h \
    ../wpmcpp/wpmutils.h \
    ../wpmcpp/job.h \
    ../wpmcpp/version.h

