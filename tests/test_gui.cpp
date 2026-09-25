#include <QtTest>
#include <QAction>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include "mainwindow.h"
#include "diagnosticengine.h"

// Own only the simulator started by this test, including on a failed assertion.
class OwnedSimulator : public QProcess {
public:
    ~OwnedSimulator() { stop(); }
    void stop() {
        if (state() == NotRunning) return;
        terminate();
        if (!waitForFinished(2000)) { kill(); waitForFinished(2000); }
    }
};

class GuiTest : public QObject {
    Q_OBJECT
private slots:
    void virtualDeviceButtonsAndRegression();
};

void GuiTest::virtualDeviceButtonsAndRegression()
{
    OwnedSimulator simulator;
    const QDir build(QCoreApplication::applicationDirPath());
    simulator.start(QStringLiteral("python3"), {QStringLiteral("-u"),
        build.absoluteFilePath(QStringLiteral("../tools/sim_node.py")),
        QStringLiteral("--interface"), QStringLiteral("vcan0")});
    QVERIFY(simulator.waitForStarted(3000));
    QByteArray ready;
    QElapsedTimer startup;
    startup.start();
    while (!ready.contains("READY ") && startup.elapsed() < 5000 && simulator.state() != QProcess::NotRunning) {
        simulator.waitForReadyRead(100);
        ready += simulator.readAllStandardOutput();
    }
    QVERIFY2(ready.contains("READY "), simulator.readAllStandardError().constData());

    MainWindow window;
    window.show();
    QVERIFY(window.connectToInterface(QStringLiteral("vcan0")));
    auto *engine = window.findChild<DiagnosticEngine *>();
    QVERIFY(engine);
    QTRY_VERIFY_WITH_TIMEOUT(engine->online(), 2000);
    QVERIFY(engine->report().value(QStringLiteral("simulated")).toBool());
    QVERIFY(!engine->report().value(QStringLiteral("hardware_verified")).toBool());
    QSignalSpy completed(engine, &DiagnosticEngine::commandFinished);
    auto *on = window.findChild<QPushButton *>(QStringLiteral("ledOn"));
    auto *off = window.findChild<QPushButton *>(QStringLiteral("ledOff"));
    auto *query = window.findChild<QPushButton *>(QStringLiteral("queryStatus"));
    auto *save = window.findChild<QPushButton *>(QStringLiteral("saveReport"));
    auto *regression = window.findChild<QPushButton *>(QStringLiteral("runRegression"));
    auto *result = window.findChild<QLabel *>(QStringLiteral("diagnosticResult"));
    auto *disconnect = window.findChild<QAction *>(QStringLiteral("actionDisconnect"));
    QVERIFY(on && off && query && save && regression && result && disconnect);
    QVERIFY(!regression->isEnabled());

    QTest::mouseClick(on, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 1, 1500);
    QVERIFY(completed.at(0).at(2).toBool());
    QVERIFY(engine->led());
    QTest::mouseClick(query, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 2, 1500);
    QVERIFY(completed.at(1).at(2).toBool());
    QVERIFY(engine->led());
    QTest::mouseClick(off, Qt::LeftButton);
    QTRY_COMPARE_WITH_TIMEOUT(completed.size(), 3, 1500);
    QVERIFY(completed.at(2).at(2).toBool());
    QVERIFY(!engine->led());
    QTest::mouseClick(save, Qt::LeftButton);
    QVERIFY(result->text().startsWith(QStringLiteral("会话报告已保存：")));
    const QDir session(QFileInfo(engine->report().value(QStringLiteral("log_path")).toString()).absoluteDir());
    QVERIFY(session.exists(QStringLiteral("report.json")));
    QVERIFY(session.exists(QStringLiteral("report.md")));
    const QString capture = qEnvironmentVariable("CANBENCH_GUI_CAPTURE");
    if (!capture.isEmpty()) {
        QVERIFY(!QFileInfo::exists(capture));
        QVERIFY(window.captureTo(capture));
    }

    disconnect->trigger();
    QVERIFY(!engine->connected());
    QVERIFY(!on->isEnabled());
    QVERIFY(regression->isEnabled());
    simulator.stop();
    QTest::mouseClick(regression, Qt::LeftButton);
    QTRY_VERIFY_WITH_TIMEOUT(window.regressionRunning(), 2000);
    QVERIFY(!regression->isEnabled());
    // Closing during an active suite must not orphan its child processes.
    QVERIFY(!window.close());
    QTRY_VERIFY_WITH_TIMEOUT(!window.regressionRunning(), 35000);
    QVERIFY2(result->text().startsWith(QStringLiteral("模拟回归通过：")), qPrintable(result->text()));
    QVERIFY(regression->isEnabled());
    QVERIFY(window.close());
}

QTEST_MAIN(GuiTest)
#include "test_gui.moc"
