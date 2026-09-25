#include <QtTest>
#include <QLabel>
#include <QFile>
#include <QDir>
#include <QSaveFile>
#include <QJsonDocument>
#include <QDateTime>
#include "mainwindow.h"
#include "diagnosticengine.h"

static QString interfaceIndex() {
    QFile f("/sys/class/net/can0/ifindex");
    return f.open(QIODevice::ReadOnly)?QString::fromLatin1(f.readAll()).trimmed():QString();
}
class UsbRecoveryTest:public QObject {
    Q_OBJECT
private slots:
    void explicitlyCoordinatedReplug() {
        const auto out=qEnvironmentVariable("CANBENCH_USB_RECOVERY_OUTPUT");
        if(out.isEmpty())QSKIP("Requires explicit output plus a human USB replug and privileged interface helper");
        QVERIFY(!QFile::exists(out));QVERIFY(QDir().mkdir(out));
        QJsonObject record{{"physical_unplug_user_confirmation",false},{"pass",false},{"interface","can0"}};
        auto phase=[&](const QString &name){
            record["phase"]=name;record[name+"_utc"]=QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs);
            QSaveFile f(out+"/result.json");const auto data=QJsonDocument(record).toJson();
            return f.open(QIODevice::WriteOnly)&&f.write(data)==data.size()&&f.commit();
        };
        MainWindow w;w.show();QVERIFY(w.connectToInterface("can0"));
        auto *e=w.findChild<DiagnosticEngine *>();QVERIFY(e);QTRY_VERIFY_WITH_TIMEOUT(e->online(),3000);
        QVERIFY(!e->report().value("simulated").toBool());
        const auto oldIndex=interfaceIndex();QVERIFY(!oldIndex.isEmpty());record["before_ifindex"]=oldIndex;
        QSignalSpy baseline(e,&DiagnosticEngine::commandFinished);QVERIFY(e->sendGetStatus());
        QTRY_COMPARE_WITH_TIMEOUT(baseline.size(),1,1500);QVERIFY(baseline.at(0).at(2).toBool());
        record["before_log"]=e->report().value("log_path");
        QVERIFY(phase("waiting_for_unplug"));
        w.findChild<QLabel *>("recoveryResult")->setText(QStringLiteral("USB恢复验收：等待拔下CANable USB；DAP和开发板保持连接。"));
        QTRY_VERIFY_WITH_TIMEOUT(interfaceIndex().isEmpty(),300000);
        QTRY_VERIFY_WITH_TIMEOUT(!e->online(),2000);
        // A failed new query must not be presented as success while the interface is gone.
        QSignalSpy offline(e,&DiagnosticEngine::commandFinished);
        const bool accepted=e->sendGetStatus();
        if(accepted){QTRY_COMPARE_WITH_TIMEOUT(offline.size(),1,1500);QVERIFY(!offline.at(0).at(2).toBool());}
        record["offline_query_accepted"]=accepted;record["disconnected_report"]=e->report();
        QVERIFY(phase("unplug_observed"));
        w.findChild<QLabel *>("recoveryResult")->setText(QStringLiteral("已检测USB接口消失。等待CANable接回Ubuntu及can0恢复。"));
        QTRY_VERIFY_WITH_TIMEOUT(!interfaceIndex().isEmpty()&&interfaceIndex()!=oldIndex,300000);
        // Let the privileged helper raise the link before recreating the socket.
        QTest::qWait(700);QVERIFY(w.connectToInterface("can0"));
        e=w.findChild<DiagnosticEngine *>();QVERIFY(e);
        record["after_ifindex"]=interfaceIndex();QVERIFY(phase("interface_restored"));
        w.findChild<QLabel *>("recoveryResult")->setText(QStringLiteral("接口已重连，等待板端恢复（由助手检查是否需DAP复位）。"));
        QTRY_VERIFY_WITH_TIMEOUT(e->online(),120000);
        QSignalSpy restored(e,&DiagnosticEngine::commandFinished);QVERIFY(e->sendGetStatus());
        QTRY_COMPARE_WITH_TIMEOUT(restored.size(),1,1500);QVERIFY(restored.at(0).at(2).toBool());
        record["after_log"]=e->report().value("log_path");record["restored_report"]=e->report();record["pass"]=true;
        QVERIFY(phase("passed"));
        w.findChild<QLabel *>("recoveryResult")->setText(QStringLiteral("USB接口消失、重建接口、重连及新查询通过。"));
        QVERIFY(w.captureTo(out+"/passed.png"));qInfo().noquote()<<"USB_RECOVERY_REPORT"<<out;
        QVERIFY(w.close());
    }
};
QTEST_MAIN(UsbRecoveryTest)
#include "test_physical_usb_recovery.moc"
