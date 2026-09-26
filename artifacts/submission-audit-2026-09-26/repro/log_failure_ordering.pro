QT += core serialbus testlib
QT -= gui
CONFIG += c++11 console testcase
TEMPLATE = app
TARGET = canbench-log-failure-ordering-tests
CAN_ROOT = $$(CANBENCH_SOURCE)
isEmpty(CAN_ROOT): error(Set CANBENCH_SOURCE to the reviewed source checkout)
INCLUDEPATH += $$quote($$CAN_ROOT/core)
SOURCES += $$quote($$PWD/test_log_failure_ordering.cpp) $$quote($$CAN_ROOT/core/diagnosticengine.cpp) $$quote($$CAN_ROOT/core/normalrun.cpp)
HEADERS += $$quote($$CAN_ROOT/core/diagnosticengine.h) $$quote($$CAN_ROOT/core/normalrun.h)
