QT       += core gui serialport widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = AutoFocusSystem
TEMPLATE = app
CONFIG += c++11

msvc {
    QMAKE_CXXFLAGS += /utf-8
    QMAKE_CFLAGS += /utf-8
}

LIBS += -LFMC4030/lib -lFMC4030-Dll

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    serialmanager.cpp \
    axiscontroller.cpp \
    liquidlenscontroller.cpp \
    workflowengine.cpp

HEADERS += \
    mainwindow.h \
    serialmanager.h \
    axiscontroller.h \
    liquidlenscontroller.h \
    workflowengine.h

FORMS += \
    mainwindow.ui
