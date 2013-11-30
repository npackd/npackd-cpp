#-------------------------------------------------
#
# Project created by QtCreator 2012-03-24T23:32:06
#
#-------------------------------------------------

QT       += network xml

QT       -= gui

TARGET = npackdlib
TEMPLATE = lib

DEFINES += NPACKDLIB_LIBRARY

SOURCES += npackdlib.cpp

HEADERS += npackdlib.h\
        npackdlib_global.h

symbian {
    MMP_RULES += EXPORTUNFROZEN
    TARGET.UID3 = 0xE8FA601B
    TARGET.CAPABILITY = 
    TARGET.EPOCALLOWDLLDATA = 1
    addFiles.sources = npackdlib.dll
    addFiles.path = !:/sys/bin
    DEPLOYMENT += addFiles
}

unix:!symbian {
    maemo5 {
        target.path = /opt/usr/lib
    } else {
        target.path = /usr/lib
    }
    INSTALLS += target
}
