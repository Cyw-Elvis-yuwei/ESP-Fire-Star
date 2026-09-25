#include <QtTest>
#include <QLabel>
#include <QPushButton>
#include <QAction>
#include <QFile>
#include <QJsonDocument>
#include "mainwindow.h"
#include "diagnosticengine.h"
#include "normalrun.h"

class PhysicalNormalTest : public QObject {
    Q_OBJECT
private slots:
    void explicitlyRequestedHardwareRun() {
        const QString iface = qEnvironmentVariable("CANBENCH_PHYSICAL_INTERFACE");
        if (iface.isEmpty()) QSKIP("Physical control requires explicit CANBENCH_PHYSICAL_INTERFACE");
        MainWindow window; window.show();
        QVERIFY(window.connectToInterface(iface));
        auto *engine = window.findChild<DiagnosticEngine *>();
        auto *run = window.findChild<NormalRun *>();
        QVERIFY(engine && run);
        QVERIFY(!engine->report().value("simulated").toBool());
        QTRY_VERIFY_WITH_TIMEOUT(engine->online(), 2000);
        auto *button = window.findChild<QPushButton *>("runNormal");
        auto *on = window.findChild<QPushButton *>("ledOn");
        auto *result = window.findChild<QLabel *>("normalResult");
        auto *disconnect = window.findChild<QAction *>("actionDisconnect");
        QVERIFY(button && on && result && disconnect); QVERIFY(button->isEnabled());
        const bool initial = engine->led();
        QTest::mouseClick(button, Qt::LeftButton);
        QVERIFY(run->running()); QVERIFY(!on->isEnabled()); QVERIFY(!disconnect->isEnabled());
        QVERIFY(!window.close());
        QTRY_VERIFY_WITH_TIMEOUT(!run->running(), 8000);
        QVERIFY2(run->report().value("protocol_passed").toBool(), qPrintable(result->text()));
        QVERIFY(run->report().value("report_saved").toBool());
        QCOMPARE(engine->led(), initial); QVERIFY(on->isEnabled()); QVERIFY(button->isEnabled());
        QCOMPARE(run->report().value("steps").toArray().size(), 7);
        const auto capture = qEnvironmentVariable("CANBENCH_GUI_CAPTURE");
        if (!capture.isEmpty()) QVERIFY(window.captureTo(capture));
        qInfo().noquote() << "NORMAL_REPORT" << run->directory();
        QVERIFY(window.close());
    }
};
QTEST_MAIN(PhysicalNormalTest)
#include "test_physical_normal.moc"
