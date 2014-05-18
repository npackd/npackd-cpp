TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.c

QMAKE_CXXFLAGS += -static-libgcc -Werror \
    -Wno-missing-field-initializers -Wno-unused-parameter -municode

QMAKE_LFLAGS += -static -mconsole -municode

QMAKE_LFLAGS_RELEASE += -Wl,-Map,exeproxy_release.map
