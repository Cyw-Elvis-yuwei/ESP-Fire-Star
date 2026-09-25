QT += core serialbus testlib
QT -= gui
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = canbench-continuous-tests
INCLUDEPATH += ../core
SOURCES += test_continuousrun.cpp ../core/continuousrun.cpp ../core/diagnosticengine.cpp
HEADERS += ../core/continuousrun.h ../core/diagnosticengine.h
