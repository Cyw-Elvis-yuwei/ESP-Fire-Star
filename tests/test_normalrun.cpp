#include <QtTest>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QUuid>
#include "normalrun.h"
#include "diagnosticengine.h"

struct Fixture {
    QTemporaryDir temp;
    DiagnosticEngine engine;
    NormalRun run{&engine};
    QTimer heartbeat;
    bool led = false;
    int sent = 0;
    int omit = -1;
    int wrongState = -1;
    bool wrongSequenceFirst = false;
    bool rejectSend = false;
    quint16 count = 0;
    QCanBusFrame lastReply;
    Fixture() {
        QString error;
        engine.startLog(temp.filePath("frames.jsonl"), &error);
        engine.setInterfaceName("test-only");
        engine.setSimulated(true);
        engine.setConnected(true);
        QObject::connect(&heartbeat, &QTimer::timeout, &engine, [this] { beat(); });
        heartbeat.start(80);
        beat();
        engine.setSender([this](const QCanBusFrame &frame, QString *error) {
            ++sent;
            if (rejectSend) { *error = "test transport failure"; return false; }
            auto p = frame.payload();
            if (p[1] == 1) led = p[4] != 0;
            p[4] = 0;
            p[5] = (sent == wrongState ? !led : led) ? 1 : 0;
            lastReply = QCanBusFrame(0x322, p);
            if (sent != omit) {
                auto reply = lastReply;
                QTimer::singleShot(1, &engine, [this, reply] {
                    if (wrongSequenceFirst) {
                        auto wrong = reply; auto payload = reply.payload();
                        payload[3] = char(quint8(payload[3]) ^ 0x80); wrong.setPayload(payload);
                        engine.receiveFrame(wrong);
                    }
                    engine.receiveFrame(reply);
                });
            }
            return true;
        });
    }
    void beat() {
        ++count;
        QByteArray p(8, '\0'); p[0] = 1; p[1] = led ? 1 : 0;
        p[2] = char(count); p[3] = char(count >> 8);
        quint32 uptime = quint32(count) * 80;
        for (int i = 0; i < 4; ++i) p[i+4] = char(uptime >> (8*i));
        engine.receiveFrame(QCanBusFrame(0x123, p));
    }
    QString output() const { return temp.filePath("run-" + QUuid::createUuid().toString(QUuid::WithoutBraces)); }
};

class NormalTest : public QObject {
    Q_OBJECT
private slots:
    void successRestoresState_data() { QTest::addColumn<bool>("initial"); QTest::newRow("off") << false; QTest::newRow("on") << true; }
    void successRestoresState() {
        QFETCH(bool, initial);
        Fixture f; f.led = initial; f.beat(); f.wrongSequenceFirst = true;
        QString error; const auto path = f.output();
        QVERIFY2(f.run.start(path, &error), qPrintable(error));
        QVERIFY(!f.run.start(f.output(), &error)); // No overlapping suites.
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 6000);
        const auto report = f.run.report();
        QVERIFY(report.value("protocol_passed").toBool());
        QVERIFY(report.value("report_saved").toBool());
        QCOMPARE(f.sent, 7); QCOMPARE(f.led, initial);
        QCOMPARE(report.value("initial_led").toBool(), initial);
        QVERIFY(report.value("simulated").toBool());
        const auto steps = report.value("steps").toArray();
        QCOMPARE(steps.size(), 7);
        for (const auto &step : steps) QVERIFY(step.toObject().value("success").toBool());
        QFile file(path + "/result.json"); QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(QJsonDocument::fromJson(file.readAll()).object(), report);
        QVERIFY(QFile::exists(path + "/result.md"));
        QVERIFY(!f.run.start(path, &error)); // No overwriting a previous run.
        const auto next = f.output(); QVERIFY(f.run.start(next, &error));
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 6000);
        QCOMPARE(f.sent, 14); QVERIFY(f.run.report().value("protocol_passed").toBool());
        QCOMPARE(f.run.report().value("steps").toArray().first().toObject().value("seq").toInt(), 8);
    }
    void queryMismatchStops() {
        Fixture f; f.wrongState = 3; QString error;
        QVERIFY(f.run.start(f.output(), &error));
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 4000);
        QCOMPARE(f.run.report().value("outcome").toString(), QString("query_state_mismatch"));
        QCOMPARE(f.sent, 3);
        QVERIFY(!f.run.report().value("initial_state_restored").toBool());
        QCOMPARE(f.run.report().value("steps").toArray().at(3).toObject().value("outcome").toString(), QString("not_run"));
    }
    void timeoutAndLateReplyDoNotContinue() {
        Fixture f; f.omit = 1; QString error;
        QVERIFY(f.run.start(f.output(), &error));
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 2000);
        QCOMPARE(f.run.report().value("outcome").toString(), QString("timeout"));
        auto report = f.run.report(); f.engine.receiveFrame(f.lastReply);
        QTest::qWait(450); QCOMPARE(f.sent, 1); QCOMPARE(f.run.report(), report);
    }
    void connectionAndCancellation_data() {
        QTest::addColumn<QString>("action");
        for (const char *s : {"disconnect", "cancel", "heartbeat", "transport", "restart", "log"})
            QTest::newRow(s) << QString::fromLatin1(s);
    }
    void connectionAndCancellation() {
        QFETCH(QString, action); Fixture f; QString error;
        QVERIFY(f.run.start(f.output(), &error));
        QTRY_COMPARE_WITH_TIMEOUT(f.run.report().value("outcome").toString(), QString("running"), 1000);
        QTRY_VERIFY_WITH_TIMEOUT(f.sent == 1 && !f.engine.pending(), 500);
        if (action == "disconnect") f.engine.setConnected(false);
        if (action == "cancel") f.run.cancel();
        if (action == "heartbeat") { f.heartbeat.stop(); f.omit = 2; }
        if (action == "transport") f.engine.recordTransportError("injected test failure");
        if (action == "restart") f.engine.event("restart_observed", "test event");
        if (action == "log") f.engine.startLog(f.temp.path(), &error);
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 1500);
        QVERIFY(!f.run.report().value("protocol_passed").toBool());
        int sent = f.sent; QTest::qWait(450); QCOMPARE(f.sent, sent);
        QVERIFY(f.run.report().value("report_saved").toBool());
    }
    void prerequisitesAndWriteFailure() {
        Fixture f; QString error;
        f.engine.setConnected(false); QVERIFY(!f.run.start(f.output(), &error));
        f.engine.setConnected(true); QVERIFY(!f.run.start(f.output(), &error));
        f.beat(); QVERIFY(!f.run.start(f.temp.filePath("missing/child"), &error));
        QCOMPARE(f.sent, 0);
        QVERIFY(f.run.start(f.output(), &error));
        // A write failure at the end must not be displayed as a successful saved run.
        QTRY_VERIFY_WITH_TIMEOUT(f.sent == 1 && !f.engine.pending(), 500);
        QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        QSignalSpy finished(&f.run, &NormalRun::finished);
        f.run.cancel();
        QVERIFY(!f.run.report().value("report_saved").toBool());
        QVERIFY(f.run.report().contains("report_error"));
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
        Fixture f;QString error;QVERIFY(f.run.start(f.output(), &error));
        QSignalSpy finished(&f.run, &NormalRun::finished);
        QVERIFY(QFile::remove(f.run.directory()+"/result.json"));
        QVERIFY(QDir(f.run.directory()).mkdir("result.json"));
        if (failMarkdown) QVERIFY(QDir(f.run.directory()).mkdir("result.md"));
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 6000);
        QCOMPARE(finished.size(), 1);QVERIFY(!finished.first().first().toBool());
        QVERIFY(f.run.report().value("protocol_passed").toBool());
        QVERIFY(!f.run.report().value("report_saved").toBool());
        QVERIFY(f.run.report().value("report_error").toString().contains("result.json"));
        if (failMarkdown)
            QVERIFY(f.run.report().value("report_error").toString().contains("result.md"));
        else
            QVERIFY(QFileInfo(f.run.directory()+"/result.md").isFile());
    }
    void senderFailure() {
        Fixture f; f.rejectSend = true; QString error;
        QVERIFY(f.run.start(f.output(), &error));
        QTRY_VERIFY_WITH_TIMEOUT(!f.run.running(), 1000);
        QCOMPARE(f.sent, 1);
        QVERIFY(!f.run.report().value("protocol_passed").toBool());
        QCOMPARE(f.run.report().value("outcome").toString(), QString("transport_error"));
    }
};
QTEST_GUILESS_MAIN(NormalTest)
#include "test_normalrun.moc"
