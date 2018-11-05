TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.cpp \
    csiwriter.cpp \
    csifetcher.cpp \
    csiparser.cpp

QMAKE_LFLAGS += -pthread

LIBS += /usr/lib/x86_64-linux-gnu/libarmadillo.so

HEADERS += \
    csiwriter.h \
    csifetcher.h \
    csi_packet.h \
    connector_users.h \
    iwl_connector.h \
    csiparser.h
