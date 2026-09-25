QT += core testlib
QT -= gui
CONFIG += c++11 testcase console
TEMPLATE = app
TARGET = canbench-identity-tests
SOURCES += test_socketcanidentity.cpp
HEADERS += ../core/socketcanidentity.h
