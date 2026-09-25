QT += core serialbus testlib
QT -= gui
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = canbench-recovery-tests
INCLUDEPATH += ../core
SOURCES += test_recoveryrun.cpp ../core/recoveryrun.cpp ../core/diagnosticengine.cpp
HEADERS += ../core/recoveryrun.h ../core/diagnosticengine.h
