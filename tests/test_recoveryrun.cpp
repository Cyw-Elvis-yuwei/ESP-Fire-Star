#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include "recoveryrun.h"
#include "diagnosticengine.h"
struct Fixture {
    QTemporaryDir temp;qint64 time=2000;quint16 counter=20;
    DiagnosticEngine engine{nullptr,[this]{return time;}};
    RecoveryRun run{&engine,nullptr,[this]{return time;}};
    QList<QCanBusFrame> sent;
    Fixture(){QString error;engine.startLog(temp.filePath("frames.jsonl"),&error);engine.setSimulated(true);
        engine.setInterfaceName("unit-test");engine.setConnected(true);beat();
        engine.setSender([this](const QCanBusFrame &f,QString*){sent.append(f);return true;});}
    void beat(int c=-1,int up=-1){QByteArray p(8,'\0');p[0]=1;
        quint16 n=c<0?++counter:quint16(c);quint32 u=up<0?quint32(time+100):quint32(up);
        p[2]=char(n);p[3]=char(n>>8);for(int i=0;i<4;++i)p[4+i]=char(u>>(8*i));
        engine.receiveFrame(QCanBusFrame(0x123,p));}
    void poll(){QMetaObject::invokeMethod(&run,"poll",Qt::DirectConnection);}
    bool start(){QString error;return run.start(temp.filePath("run"),&error);}
    void reply(int seq=-1){++time;auto p=sent.last().payload();p[4]=0;p[5]=0;
        if(seq>=0){p[2]=char(seq);p[3]=char(seq>>8);}engine.receiveFrame(QCanBusFrame(0x322,p));}
    void outage(){time+=501;poll();time+=500;poll();}
    void recover(){beat(1,100);time+=100;beat(2,200);poll();}
};
class RecoveryTest:public QObject{
    Q_OBJECT
private slots:
    void fullOutageTimeoutFreshQuery(){
        Fixture f;QVERIFY(f.start());f.poll();f.reply();QCOMPARE(f.run.phase(),QString("waiting_for_outage"));
        f.outage();QCOMPARE(f.run.phase(),QString("waiting_for_recovery"));QCOMPARE(f.sent.size(),2);
        QVERIFY(!f.run.report().value("protocol_passed").toBool());
        f.recover();QCOMPARE(f.run.phase(),QString("verifying_recovery"));QCOMPARE(f.sent.size(),3);
        f.reply(2);QVERIFY(f.run.running()); // Late pre-recovery reply cannot satisfy new query.
        f.reply();QVERIFY(!f.run.running());auto r=f.run.report();QVERIFY(r.value("protocol_passed").toBool());
        QVERIFY(r.value("report_saved").toBool());QVERIFY(!r.value("physical_unplug_verified").toBool());
        auto cs=r.value("commands").toArray();QCOMPARE(cs.size(),3);
        QCOMPARE(cs.at(1).toObject().value("outcome").toString(),QString("timeout"));
        QVERIFY(cs.at(2).toObject().value("success").toBool());
        QFile file(f.run.directory()+"/result.json");QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(),r);
    }
    void heartbeatAloneIsNotRecovery(){
        Fixture f;QVERIFY(f.start());f.poll();f.reply();f.outage();f.recover();
        QVERIFY(f.run.running());QVERIFY(!f.run.report().value("protocol_passed").toBool());
        f.time+=500;f.poll();QVERIFY(!f.run.running());
        QCOMPARE(f.run.report().value("outcome").toString(),QString("recovery_query_failed"));
    }
    void restoredTooSoonDoesNotPass(){
        Fixture f;QVERIFY(f.start());f.poll();f.reply();f.time+=501;f.poll();f.reply();
        QVERIFY(!f.run.running());QCOMPARE(f.run.report().value("outcome").toString(),QString("expected_timeout_not_observed"));
    }
    void noOutageAndNoRecoveryDeadlines(){
        Fixture f;QVERIFY(f.start());f.poll();f.reply();
        for(int i=0;i<600;++i){f.time+=100;f.beat();f.poll();}
        QCOMPARE(f.run.report().value("outcome").toString(),QString("stage_deadline"));
        Fixture g;QVERIFY(g.start());g.poll();g.reply();g.outage();g.time+=60000;g.poll();
        QCOMPARE(g.run.report().value("outcome").toString(),QString("stage_deadline"));
    }
    void cannotStartUnreadyOrOverwrite(){
        Fixture f;QString error;f.engine.setConnected(false);QVERIFY(!f.start());
        f.engine.setConnected(true);QVERIFY(!f.start());f.beat();QVERIFY(f.start());
        QVERIFY(!f.run.start(f.temp.filePath("other"),&error));f.run.cancel();
        QVERIFY(!f.run.start(f.run.directory(),&error));QVERIFY(!f.run.report().value("protocol_passed").toBool());
    }
    void faults_data(){QTest::addColumn<QString>("action");for(const char*s:{"adapter","transport","log","cancel"})QTest::newRow(s)<<QString(s);}
    void faults(){QFETCH(QString,action);Fixture f;QString error;QVERIFY(f.start());f.poll();f.reply();
        if(action=="adapter")f.engine.setConnected(false);
        if(action=="transport")f.engine.recordTransportError("test");
        if(action=="log")f.engine.startLog(f.temp.path(),&error);
        if(action=="cancel")f.run.cancel();
        QVERIFY(!f.run.running());QVERIFY(!f.run.report().value("protocol_passed").toBool());
        int n=f.sent.size();f.time+=1000;f.poll();QCOMPARE(f.sent.size(),n);}
    void reportFailure(){Fixture f;QVERIFY(f.start());f.poll();f.reply();QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        QSignalSpy finished(&f.run, &RecoveryRun::finished);
        f.run.cancel();QVERIFY(!f.run.report().value("report_saved").toBool());
        QCOMPARE(finished.size(), 1);QVERIFY(!finished.first().first().toBool());
        QFile file(f.run.directory()+"/result.json");QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(), f.run.report());
        QVERIFY(f.run.report().value("report_error").toString().contains("result.md"));
    }
    void jsonWriteFailureNeverReportsSaved_data() {
        QTest::addColumn<bool>("failMarkdown");
        QTest::newRow("json-only") << false;
        QTest::newRow("both-files") << true;
    }
    void jsonWriteFailureNeverReportsSaved(){
        QFETCH(bool, failMarkdown);
        Fixture f;QVERIFY(f.start());f.poll();f.reply();f.outage();f.recover();
        QSignalSpy finished(&f.run, &RecoveryRun::finished);
        QVERIFY(QFile::remove(f.run.directory()+"/result.json"));
        QVERIFY(QDir(f.run.directory()).mkdir("result.json"));
        if (failMarkdown) QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        f.reply();QVERIFY(!f.run.running());
        QCOMPARE(finished.size(), 1);QVERIFY(!finished.first().first().toBool());
        QVERIFY(f.run.report().value("protocol_passed").toBool());
        QVERIFY(!f.run.report().value("report_saved").toBool());
        QVERIFY(f.run.report().value("report_error").toString().contains("result.json"));
        if (failMarkdown)
            QVERIFY(f.run.report().value("report_error").toString().contains("result.md"));
        else
            QVERIFY(QFileInfo(f.run.directory()+"/result.md").isFile());
    }
};
QTEST_GUILESS_MAIN(RecoveryTest)
#include "test_recoveryrun.moc"
