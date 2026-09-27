#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include "continuousrun.h"
#include "diagnosticengine.h"

struct Fixture {
    QTemporaryDir temp;
    qint64 time = 0;
    DiagnosticEngine engine{nullptr, [this] { return time; }};
    ContinuousRun run{&engine, nullptr, [this] { return time; }};
    QList<QCanBusFrame> sent;
    bool reject = false;
    quint16 heartbeatCount = 0;
    Fixture() {
        QString error;
        engine.startLog(temp.filePath("frames.jsonl"), &error);
        engine.setInterfaceName("unit-test"); engine.setSimulated(true); engine.setConnected(true);
        beat();
        engine.setSender([this](const QCanBusFrame &f, QString *error) {
            sent.append(f);
            if (reject) { *error = "injected transport error"; return false; }
            return true;
        });
    }
    void beat() {
        QByteArray p(8, '\0');p[0]=1; ++heartbeatCount;
        p[2]=char(heartbeatCount);p[3]=char(heartbeatCount>>8);
        quint32 up=quint32(time+100);
        for(int i=0;i<4;++i)p[i+4]=char(up>>(8*i));
        engine.receiveFrame(QCanBusFrame(0x123,p));
    }
    void poll() { QMetaObject::invokeMethod(&run,"poll",Qt::DirectConnection); }
    void advance(int ms) { time+=ms;beat();poll(); }
    void reply(int delay, int offset=0) {
        time+=delay;
        auto p=sent.last().payload();p[4]=0;p[5]=0;
        if(offset)p[3]=char(quint8(p[3])^0x80);
        engine.receiveFrame(QCanBusFrame(0x322,p));
    }
    bool start(int duration=60000) { QString error;return run.start(temp.filePath("run"),&error,duration); }
};

class ContinuousTest: public QObject {
    Q_OBJECT
private slots:
    void fullDurationAndAllSamples() {
        Fixture f;QVERIFY(f.start());f.poll();
        for(int i=0;i<600;++i) {
            QCOMPARE(f.sent.size(),i+1);
            QCOMPARE(quint8(f.sent.last().payload()[1]),quint8(2)); // Query only.
            if(i%50==0)f.reply(0,1); // An unrelated reply cannot finish a request.
            const int latency=i%5+1;f.reply(latency);f.advance(100-latency);
        }
        QVERIFY(!f.run.running());const auto r=f.run.report();
        QVERIFY(r.value("protocol_passed").toBool());QVERIFY(r.value("full_duration_completed").toBool());
        QCOMPARE(r.value("attempts").toInt(),600);QCOMPARE(r.value("succeeded").toInt(),600);
        QCOMPARE(r.value("failed").toInt(),0);QCOMPARE(r.value("elapsed_ms").toInt(),60000);
        QCOMPARE(r.value("samples").toArray().size(),600); // Independent of engine's 256-item history.
        QVERIFY(f.engine.counters().value("results_dropped").toInt()>0);
        const auto latency=r.value("latency_ms").toObject();
        QCOMPARE(latency.value("min").toInt(),1);QCOMPARE(latency.value("max").toInt(),5);
        QCOMPARE(latency.value("mean").toDouble(),3.0);QCOMPARE(latency.value("p95").toInt(),5);
        QCOMPARE(r.value("success_rate_percent").toDouble(),100.0);
        QVERIFY(r.value("report_saved").toBool());
        QFile file(f.run.directory()+"/result.json");QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(),r);
        QFile md(f.run.directory()+"/result.md");QVERIFY(md.open(QIODevice::ReadOnly));
        QVERIFY(md.readAll().contains("100.00%"));
        QString error;QVERIFY(!f.run.start(f.run.directory(),&error,200));
        QVERIFY(f.run.start(f.temp.filePath("second"),&error,200));f.poll();f.reply(1);f.advance(99);f.reply(1);f.advance(99);
        QCOMPARE(f.run.report().value("attempts").toInt(),2);QVERIFY(f.run.report().value("protocol_passed").toBool());
    }
    void timeoutCountAndLateReply() {
        Fixture f;QVERIFY(f.start(1000));f.poll();const auto first=f.sent.first().payload();
        for(int i=0;i<5;++i)f.advance(100);
        QCOMPARE(f.sent.size(),2); // No second request while first was outstanding.
        auto late=first;late[4]=0;f.engine.receiveFrame(QCanBusFrame(0x322,late));
        QVERIFY(f.engine.pending());
        for(int i=0;i<5;++i) {f.reply(10);f.advance(90);}
        const auto r=f.run.report();QVERIFY(!f.run.running());
        QCOMPARE(r.value("outcome").toString(),QString("completed_with_failures"));
        QVERIFY(!r.value("protocol_passed").toBool());QVERIFY(r.value("full_duration_completed").toBool());
        QCOMPARE(r.value("attempts").toInt(),6);QCOMPARE(r.value("succeeded").toInt(),5);
        QCOMPARE(r.value("failed").toInt(),1);QCOMPARE(r.value("timeouts").toInt(),1);
        QCOMPARE(r.value("latency_ms").toObject().value("max").toInt(),10); // Excludes failed 500ms timeout.
    }
    void cancelSettlesPendingAndNeverPasses() {
        Fixture f;QVERIFY(f.start(500));f.poll();f.run.cancel();QVERIFY(f.run.running());
        f.advance(100);QCOMPARE(f.sent.size(),1);f.reply(5);
        QVERIFY(!f.run.running());auto r=f.run.report();
        QCOMPARE(r.value("outcome").toString(),QString("cancelled"));QVERIFY(!r.value("protocol_passed").toBool());
        QCOMPARE(r.value("attempts").toInt(),1);QCOMPARE(r.value("succeeded").toInt(),1);
        QCOMPARE(r.value("success_rate_percent").toDouble(),100.0);QVERIFY(!r.value("full_duration_completed").toBool());
        f.advance(100);QCOMPARE(f.sent.size(),1);QCOMPARE(f.run.report(),r);
    }
    void lastReplyCanSettleAfterDuration() {
        Fixture f;QVERIFY(f.start(200));f.poll();f.reply(1);f.advance(99);
        f.advance(100);QVERIFY(f.run.running());QCOMPARE(f.sent.size(),2);
        f.reply(20);f.poll();QVERIFY(!f.run.running());QVERIFY(f.run.report().value("protocol_passed").toBool());
        QCOMPARE(f.run.report().value("elapsed_ms").toInt(),220);
    }
    void errors_data() {
        QTest::addColumn<QString>("action");
        for(const char *s:{"transport","disconnect","heartbeat","restart","log"})QTest::newRow(s)<<QString(s);
    }
    void errors() {
        QFETCH(QString,action);Fixture f;QString error;QVERIFY(f.start());f.poll();
        if(action=="transport")f.engine.recordTransportError("injected");
        if(action=="disconnect")f.engine.setConnected(false);
        if(action=="heartbeat"){f.time=600;f.poll();}
        if(action=="restart"){f.engine.event("restart_observed","injected");f.reply(1);}
        if(action=="log"){f.engine.startLog(f.temp.path(),&error);f.reply(1);}
        QVERIFY(!f.run.running());QVERIFY(!f.run.report().value("protocol_passed").toBool());
        QCOMPARE(f.run.report().value("attempts").toInt(),1);
        QCOMPARE(f.run.report().value("samples").toArray().size(),1);
        f.advance(100);QCOMPARE(f.sent.size(),1);
    }
    void gatesAndReportFailure() {
        Fixture f;QString error;
        QVERIFY(!f.run.start(f.temp.filePath("bad"),&error,60001));
        f.engine.setConnected(false);QVERIFY(!f.start());f.engine.setConnected(true);QVERIFY(!f.start());f.beat();
        QVERIFY(!f.run.start(f.temp.filePath("missing/child"),&error));
        QVERIFY(f.start(200));QVERIFY(!f.run.start(f.temp.filePath("overlap"),&error));
        QSignalSpy finished(&f.run, &ContinuousRun::finished);
        f.poll();f.reply(1);QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        f.advance(99);f.reply(1);f.advance(99);
        QVERIFY(!f.run.report().value("report_saved").toBool());QVERIFY(f.run.report().contains("report_error"));
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
    void jsonWriteFailureNeverReportsSaved() {
        QFETCH(bool, failMarkdown);
        Fixture f;QVERIFY(f.start(200));QSignalSpy finished(&f.run, &ContinuousRun::finished);
        f.poll();f.reply(1);
        QVERIFY(QFile::remove(f.run.directory()+"/result.json"));
        QVERIFY(QDir(f.run.directory()).mkdir("result.json"));
        if (failMarkdown) QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        f.advance(99);f.reply(1);f.advance(99);
        QCOMPARE(finished.size(), 1);QVERIFY(!finished.first().first().toBool());
        QVERIFY(f.run.report().value("protocol_passed").toBool());
        QVERIFY(!f.run.report().value("report_saved").toBool());
        QVERIFY(f.run.report().value("report_error").toString().contains("result.json"));
        if (failMarkdown)
            QVERIFY(f.run.report().value("report_error").toString().contains("result.md"));
        else
            QVERIFY(QFileInfo(f.run.directory()+"/result.md").isFile());
    }
    void synchronousSendFailureCountedOnce() {
        Fixture f;f.reject=true;QVERIFY(f.start());f.poll();QVERIFY(!f.run.running());
        const auto r=f.run.report();QCOMPARE(r.value("attempts").toInt(),1);QCOMPARE(r.value("failed").toInt(),1);
        QCOMPARE(r.value("outcome").toString(),QString("transport_error"));QVERIFY(r.value("latency_ms").isNull());
    }
};
QTEST_GUILESS_MAIN(ContinuousTest)
#include "test_continuousrun.moc"
