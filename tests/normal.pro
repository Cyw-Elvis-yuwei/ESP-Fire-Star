QT += core serialbus testlib
QT -= gui
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = canbench-normal-tests
INCLUDEPATH += ../core
SOURCES += test_normalrun.cpp ../core/normalrun.cpp ../core/diagnosticengine.cpp
HEADERS += ../core/normalrun.h ../core/diagnosticengine.h
