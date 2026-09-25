#include <QtTest>
#include <QAction>
#include <QPushButton>
#include <QLabel>
#include <QSaveFile>
#include <QJsonDocument>
#include "mainwindow.h"
#include "diagnosticengine.h"
#include "normalrun.h"
#include "continuousrun.h"
#include "recoveryrun.h"
class PhysicalRecoveryTest:public QObject{
    Q_OBJECT
private slots:
    void coordinatedNodeOutage(){
        const auto iface=qEnvironmentVariable("CANBENCH_PHYSICAL_INTERFACE");
        const auto ready=qEnvironmentVariable("CANBENCH_RECOVERY_READY");
        if(iface.isEmpty()||ready.isEmpty())QSKIP("Explicit physical interface and external fault coordinator required");
        QVERIFY(!QFile::exists(ready));
        MainWindow w;w.show();QVERIFY(w.connectToInterface(iface));
        auto *e=w.findChild<DiagnosticEngine *>();auto *run=w.findChild<RecoveryRun *>();
        QVERIFY(e&&run);QVERIFY(!e->report().value("simulated").toBool());QTRY_VERIFY_WITH_TIMEOUT(e->online(),2000);
        auto *b=w.findChild<QPushButton *>("runRecovery");auto *normal=w.findChild<QPushButton *>("runNormal");
        auto *continuous=w.findChild<QPushButton *>("runContinuous");auto *led=w.findChild<QPushButton *>("ledOn");
        auto *result=w.findChild<QLabel *>("recoveryResult");QVERIFY(b&&normal&&continuous&&led&&result);
        QTest::mouseClick(b,Qt::LeftButton);QVERIFY(run->running());QVERIFY(!normal->isEnabled()&&!continuous->isEnabled()&&!led->isEnabled());
        QMetaObject::invokeMethod(&w,"runNormal",Qt::DirectConnection);QMetaObject::invokeMethod(&w,"runContinuous",Qt::DirectConnection);
        QVERIFY(!w.findChild<NormalRun *>()->running()&&!w.findChild<ContinuousRun *>()->running());
        QVERIFY(!w.close());
        QTRY_COMPARE_WITH_TIMEOUT(run->phase(),QString("waiting_for_outage"),2000);
        QSaveFile f(ready);QVERIFY(f.open(QIODevice::WriteOnly));
        const auto bytes=QJsonDocument(QJsonObject{{"phase",run->phase()},{"directory",run->directory()}}).toJson();
        QCOMPARE(f.write(bytes),qint64(bytes.size()));QVERIFY(f.commit());
        QTRY_VERIFY_WITH_TIMEOUT(!run->running(),70000);
        QVERIFY2(run->report().value("protocol_passed").toBool(),qPrintable(result->text()));
        QVERIFY(run->report().value("report_saved").toBool());QCOMPARE(run->report().value("commands").toArray().size(),3);
        QVERIFY(e->online());QVERIFY(normal->isEnabled()&&continuous->isEnabled()&&led->isEnabled());
        const auto capture=qEnvironmentVariable("CANBENCH_GUI_CAPTURE");if(!capture.isEmpty())QVERIFY(w.captureTo(capture));
        qInfo().noquote()<<"RECOVERY_REPORT"<<run->directory();QVERIFY(w.close());
    }
};
QTEST_MAIN(PhysicalRecoveryTest)
#include "test_physical_recovery.moc"
