#include <QtTest>
#include <QTemporaryDir>
#include <QJsonObject>
#include "diagnosticengine.h"
#include "normalrun.h"

// Controlled error/re-entrancy probe. No SocketCAN backend or hardware is opened.
// An expected failure on an unpatched version is not proof of a physical outage.
class LogFailureOrderingTest : public QObject {
    Q_OBJECT
private slots:
    void noSubmissionAfterRunStopsForLogFailure() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        DiagnosticEngine engine;
        NormalRun run(&engine);
        QString error;
        QVERIFY(engine.startLog(temp.filePath("frames.jsonl"), &error));
        engine.setInterfaceName("fake-transport-only");
        engine.setSimulated(true);
        engine.setConnected(true);
        QByteArray heartbeat(8, '\0');
        heartbeat[0] = 1;
        heartbeat[2] = 1;
        heartbeat[4] = 100;
        engine.receiveFrame(QCanBusFrame(0x123, heartbeat));
        QVERIFY(engine.online());

        int submissions = 0;
        bool failureInjected = false;
        bool injectionFailedAsExpected = false;
        bool finishedSeen = false;
        bool submittedAfterFinished = false;
        QStringList trace;
        QObject::connect(&run, &NormalRun::finished, &engine,
            [&](bool, const QString &) {
                finishedSeen = true;
                trace << "normal_run_finished";
            });
        engine.setSender([&](const QCanBusFrame &, QString *) {
            ++submissions;
            submittedAfterFinished = finishedSeen;
            trace << "fake_sender_called";
            return true;
        });
        QObject::connect(&engine, &DiagnosticEngine::pendingChanged, &engine,
            [&](bool pending) {
                if (!pending || failureInjected) return;
                failureInjected = true;
                trace << "inject_log_open_failure";
                QString why;
                // Opening a directory for a file log fails. This deliberately
                // raises the real log_error path before sendCommand calls sender.
                injectionFailedAsExpected = !engine.startLog(temp.path(), &why);
            });
        QVERIFY(run.start(temp.filePath("normal-case"), &error));
        QTRY_VERIFY_WITH_TIMEOUT(!run.running(), 1000);
        QVERIFY(failureInjected);
        QVERIFY(injectionFailedAsExpected);
        QVERIFY(finishedSeen);
        QCOMPARE(run.report().value("outcome").toString(), QString("log_error"));
        qInfo() << "TRACE" << trace << "SUBMISSIONS" << submissions
                << "AFTER_FINISHED" << submittedAfterFinished;
        // Contract proposed by the audit: a request not yet submitted when
        // logging aborts the run must not be submitted afterwards.
        QCOMPARE(submissions, 0);
    }
};
QTEST_GUILESS_MAIN(LogFailureOrderingTest)
#include "test_log_failure_ordering.moc"
