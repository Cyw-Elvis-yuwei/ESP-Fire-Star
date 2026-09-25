#include <QtTest>
#include <QAction>
#include <QLabel>
#include <QPushButton>
#include "mainwindow.h"
#include "diagnosticengine.h"
#include "continuousrun.h"
#include "normalrun.h"

class PhysicalContinuousTest:public QObject {
    Q_OBJECT
private slots:
    void explicitlyRequestedSixtySeconds() {
        const auto iface=qEnvironmentVariable("CANBENCH_PHYSICAL_INTERFACE");
        if(iface.isEmpty())QSKIP("Physical run requires explicit CANBENCH_PHYSICAL_INTERFACE");
        MainWindow window;window.show();QVERIFY(window.connectToInterface(iface));
        auto *engine=window.findChild<DiagnosticEngine *>();auto *run=window.findChild<ContinuousRun *>();
        auto *normal=window.findChild<NormalRun *>();QVERIFY(engine&&run&&normal);
        QVERIFY(!engine->report().value("simulated").toBool());QTRY_VERIFY_WITH_TIMEOUT(engine->online(),2000);
        auto *button=window.findChild<QPushButton *>("runContinuous");auto *normalButton=window.findChild<QPushButton *>("runNormal");
        auto *led=window.findChild<QPushButton *>("ledOn");auto *result=window.findChild<QLabel *>("continuousResult");
        QVERIFY(button&&normalButton&&led&&result);
        // Regression of the existing button and mutual exclusion in both directions.
        QTest::mouseClick(normalButton,Qt::LeftButton);QVERIFY(normal->running());QVERIFY(!button->isEnabled());
        QMetaObject::invokeMethod(&window,"runContinuous",Qt::DirectConnection);QVERIFY(!run->running());
        QTRY_VERIFY_WITH_TIMEOUT(!normal->running(),8000);QVERIFY(normal->report().value("protocol_passed").toBool());
        const bool initialLed=engine->led();int uiTicks=0;QTimer responsiveness;
        QObject::connect(&responsiveness,&QTimer::timeout,&window,[&]{++uiTicks;});responsiveness.start(100);
        QTest::mouseClick(button,Qt::LeftButton);QVERIFY(run->running());QVERIFY(!normalButton->isEnabled());QVERIFY(!led->isEnabled());
        QMetaObject::invokeMethod(&window,"runNormal",Qt::DirectConnection);QVERIFY(!normal->running());
        QVERIFY(!window.close());
        QTRY_VERIFY_WITH_TIMEOUT(!run->running(),65000);
        QVERIFY2(run->report().value("protocol_passed").toBool(),qPrintable(result->text()));
        QVERIFY(run->report().value("report_saved").toBool());QVERIFY(run->report().value("full_duration_completed").toBool());
        QCOMPARE(run->report().value("failed").toInt(),0);QVERIFY(run->report().value("attempts").toInt()>256);
        QCOMPARE(engine->led(),initialLed);QVERIFY(normalButton->isEnabled());QVERIFY(led->isEnabled());QVERIFY(uiTicks>200);
        const auto capture=qEnvironmentVariable("CANBENCH_GUI_CAPTURE");if(!capture.isEmpty())QVERIFY(window.captureTo(capture));
        qInfo().noquote()<<"CONTINUOUS_REPORT"<<run->directory();qInfo()<<"UI_TIMER_TICKS"<<uiTicks;
        QVERIFY(window.close());
    }
};
QTEST_MAIN(PhysicalContinuousTest)
#include "test_physical_continuous.moc"
