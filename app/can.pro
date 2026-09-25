QT += serialbus widgets
requires(qtConfig(combobox))

TARGET = can-diagnostic-bench
TEMPLATE = app
CONFIG += c++11
INCLUDEPATH += ../core

SOURCES += \
    bitratebox.cpp \
    connectdialog.cpp \
    main.cpp \
    mainwindow.cpp \
    sendframebox.cpp \
    ../core/diagnosticengine.cpp \
    ../core/normalrun.cpp \
    ../core/continuousrun.cpp \
    ../core/recoveryrun.cpp

HEADERS += \
    bitratebox.h \
    connectdialog.h \
    mainwindow.h \
    sendframebox.h \
    ../core/diagnosticengine.h \
    ../core/socketcanidentity.h \
    ../core/normalrun.h \
    ../core/continuousrun.h \
    ../core/recoveryrun.h

FORMS   += mainwindow.ui \
    connectdialog.ui \
    sendframebox.ui

RESOURCES += can.qrc

target.path = $$[QT_INSTALL_EXAMPLES]/serialbus/can
INSTALLS += target
