TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

DEPENDPATH += /s/shared/ezcpp
INCLUDEPATH += /s/shared/ezcpp

SOURCES += \
        ../DFRobotDFPlayerMini.cpp \
        main.cpp

HEADERS += \
  ../DFRobotDFPlayerMini.h \
  Arduino.h
