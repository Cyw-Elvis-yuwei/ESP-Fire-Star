QT += widgets serialbus testlib
CONFIG += c++11 testcase
TARGET = canbench-gui-tests
TEMPLATE = app
INCLUDEPATH += ../app ../core
SOURCES += test_gui.cpp ../app/bitratebox.cpp ../app/connectdialog.cpp \
           ../app/mainwindow.cpp ../app/sendframebox.cpp ../core/diagnosticengine.cpp ../core/normalrun.cpp ../core/continuousrun.cpp ../core/recoveryrun.cpp
HEADERS += ../app/bitratebox.h ../app/connectdialog.h ../app/mainwindow.h \
           ../app/sendframebox.h ../core/diagnosticengine.h ../core/normalrun.h ../core/continuousrun.h ../core/recoveryrun.h
FORMS += ../app/mainwindow.ui ../app/connectdialog.ui ../app/sendframebox.ui
RESOURCES += ../app/can.qrc
