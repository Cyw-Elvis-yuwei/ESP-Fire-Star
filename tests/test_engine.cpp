#include <QtTest>
#include <QFile>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#ifdef Q_OS_LINUX
#include <fcntl.h>
#include <unistd.h>
#endif
#include "diagnosticengine.h"

namespace {
QCanBusFrame response(quint16 seq, quint8 op = 2, quint8 result = 0, bool led = false)
{
    QByteArray p(8, '\0');
    p[0] = 1; p[1] = char(op); p[2] = char(seq & 255); p[3] = char(seq >> 8);
    p[4] = char(result); p[5] = led ? 1 : 0;
    return QCanBusFrame(0x322, p);
}
QCanBusFrame heartbeat(quint16 counter, quint32 uptime, bool led = false)
{
    QByteArray p(8, '\0');
    p[0] = 1; p[1] = led ? 1 : 0; p[2] = char(counter & 255); p[3] = char(counter >> 8);
    for (int i = 0; i < 4; ++i) p[4 + i] = char(uptime >> (8 * i));
    return QCanBusFrame(0x123, p);
}
int counter(const DiagnosticEngine &engine, const char *name)
{
    return engine.counters().value(QLatin1String(name)).toInt();
}
QString lastOutcome(const DiagnosticEngine &engine)
{
    return engine.report().value(QStringLiteral("command_results")).toArray().last().toObject()
            .value(QStringLiteral("outcome")).toString();
}
void connectEngine(DiagnosticEngine &engine)
{
    engine.setSender([](const QCanBusFrame &, QString *) { return true; });
    engine.setConnected(true);
}
}

class EngineTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase();
    void requestEncodingAndSinglePending();
    void responseMatchingAndDuplicates();
    void malformedResponse_data();
    void malformedResponse();
    void responseErrorsAndLedMismatch();
    void deadlineAndNewSequence();
    void heartbeatFreshnessAndDeadline();
    void heartbeatFlagsAndFields();
    void rolloverIsNotRestart();
    void staleHeartbeatSequenceDoesNotRestart();
    void conservativeRestartAndRecovery();
    void disconnectAndTransportFailure();
    void boundedHistoryAndReports();
    void failedLogIsVisible();
    void submissionStopsWhenLoggingFails_data();
    void submissionStopsWhenLoggingFails();
    void failedSessionRejectsNewSubmission();
    void submissionCancelledByDisconnect();
    void pendingCallbackReplacesRequest();
    void senderRemovedBeforeSubmission();
    void noLogSynchronousReplyStillWorks();
    void loggingFailureAfterSubmissionDoesNotUndoTransport();
};

void EngineTest::initTestCase()
{
    qRegisterMetaType<quint16>("quint16");
    qRegisterMetaType<quint8>("quint8");
    qRegisterMetaType<quint32>("quint32");
}

void EngineTest::requestEncodingAndSinglePending()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    QList<QCanBusFrame> sent;
    engine.setSender([&](const QCanBusFrame &f, QString *) { sent.append(f); return true; });
    QVERIFY(!engine.sendGetStatus());
    engine.setConnected(true);
    QVERIFY(!engine.online());
    QVERIFY(engine.sendSetLed(true));
    QVERIFY(engine.pending());
    QCOMPARE(sent.size(), 1);
    QCOMPARE(sent.first().frameId(), quint32(0x321));
    QCOMPARE(sent.first().payload(), QByteArray::fromHex("0101010001000000"));
    QVERIFY(!engine.sendGetStatus());
    QCOMPARE(sent.size(), 1);
    time = 42;
    engine.receiveFrame(response(engine.pendingSequence(), 1, 0, true));
    QVERIFY(!engine.pending());
    QVERIFY(engine.led());
    QVERIFY(!engine.online()); // A matching response cannot substitute for a heartbeat.
    QVERIFY(engine.sendGetStatus());
    QCOMPARE(sent.last().payload(), QByteArray::fromHex("0102020000000000"));
    QCOMPARE(counter(engine, "commands_rejected"), 2);
}

void EngineTest::responseMatchingAndDuplicates()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    QSignalSpy results(&engine, &DiagnosticEngine::commandFinished);
    QVERIFY(engine.sendGetStatus());
    const quint16 first = engine.pendingSequence();
    engine.receiveFrame(response(first + 1));
    engine.receiveFrame(response(first, 1));
    QCanBusFrame echo = response(first);
    echo.setLocalEcho(true);
    engine.receiveFrame(echo);
    QVERIFY(engine.pending());
    QCOMPARE(results.size(), 0);
    time = 37;
    engine.receiveFrame(response(first));
    QCOMPARE(results.size(), 1);
    QCOMPARE(results.first().at(3).toString(), QStringLiteral("success"));
    QCOMPARE(results.first().at(4).toLongLong(), qint64(37));
    engine.receiveFrame(response(first));
    QVERIFY(engine.sendGetStatus());
    engine.receiveFrame(response(first));
    QVERIFY(engine.pending());
    engine.receiveFrame(response(engine.pendingSequence()));
    QCOMPARE(counter(engine, "commands_succeeded"), 2);
    QCOMPARE(counter(engine, "unmatched_responses"), 4);
    QCOMPARE(counter(engine, "local_echo_frames"), 1);
}

void EngineTest::malformedResponse_data()
{
    QTest::addColumn<int>("mutation");
    const char *names[] = {"short", "long", "version", "unsupported_op", "result", "led",
                          "reserved6", "reserved7", "extended", "fd", "rtr", "error",
                          "brs", "esi"};
    for (int i = 0; i < 14; ++i) QTest::newRow(names[i]) << i;
}

void EngineTest::malformedResponse()
{
    QFETCH(int, mutation);
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    QVERIFY(engine.sendGetStatus());
    QCanBusFrame f = response(engine.pendingSequence());
    QByteArray p = f.payload();
    switch (mutation) {
    case 0: p.resize(7); break;
    case 1: p.append('\0'); break;
    case 2: p[0] = 2; break;
    case 3: p[1] = 3; break;
    case 4: p[4] = 4; break;
    case 5: p[5] = 2; break;
    case 6: p[6] = 1; break;
    case 7: p[7] = 1; break;
    case 8: f.setExtendedFrameFormat(true); break;
    case 9: f.setFlexibleDataRateFormat(true); break;
    case 10: f.setFrameType(QCanBusFrame::RemoteRequestFrame); break;
    case 11: f.setFrameType(QCanBusFrame::ErrorFrame); break;
    case 12: f.setBitrateSwitch(true); break;
    case 13: f.setErrorStateIndicator(true); break;
    }
    f.setPayload(p);
    engine.receiveFrame(f);
    QVERIFY(engine.pending());
    QCOMPARE(counter(engine, "commands_succeeded"), 0);
    // Qt's ErrorFrame stores an error mask in place of the ID, so it can be ignored by ID.
    QVERIFY(counter(engine, "invalid_frames") + counter(engine, "ignored_frames") == 1);
    engine.receiveFrame(response(engine.pendingSequence()));
    QCOMPARE(counter(engine, "commands_succeeded"), 1);
}

void EngineTest::responseErrorsAndLedMismatch()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    for (quint8 error = 1; error <= 3; ++error) {
        QVERIFY(engine.sendGetStatus());
        engine.receiveFrame(response(engine.pendingSequence(), 2, error));
        QVERIFY(!engine.pending());
        QCOMPARE(lastOutcome(engine), QStringLiteral("response_error_%1").arg(error));
    }
    QVERIFY(engine.sendSetLed(true));
    engine.receiveFrame(response(engine.pendingSequence(), 1, 0, false));
    QCOMPARE(lastOutcome(engine), QStringLiteral("state_mismatch"));
    QVERIFY(!engine.led());
    QCOMPARE(counter(engine, "commands_failed"), 4);
    QCOMPARE(counter(engine, "commands_succeeded"), 0);
}

void EngineTest::deadlineAndNewSequence()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    QVERIFY(engine.sendGetStatus());
    time = 499;
    engine.receiveFrame(response(engine.pendingSequence()));
    QCOMPARE(counter(engine, "commands_succeeded"), 1);
    QVERIFY(engine.sendGetStatus());
    const quint16 expired = engine.pendingSequence();
    time = 999; // Exactly 500 ms; no timer callback has been dispatched.
    engine.receiveFrame(response(expired));
    QCOMPARE(lastOutcome(engine), QStringLiteral("timeout"));
    QCOMPARE(counter(engine, "commands_succeeded"), 1);
    QCOMPARE(counter(engine, "commands_timeouts"), 1);
    QVERIFY(engine.sendGetStatus());
    QVERIFY(engine.pendingSequence() != expired);
    engine.receiveFrame(response(expired));
    QVERIFY(engine.pending());
    time = 1600;
    engine.checkTimeouts();
    QCOMPARE(counter(engine, "commands_timeouts"), 2);
    QCOMPARE(engine.report().value("command_results").toArray().last().toObject()
             .value("latency_ms").toInt(), 601);
}

void EngineTest::heartbeatFreshnessAndDeadline()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    engine.receiveFrame(heartbeat(10, 1000));
    QVERIFY(engine.online());
    time = 400;
    engine.receiveFrame(heartbeat(10, 1000));
    engine.receiveFrame(heartbeat(9, 900));
    engine.receiveFrame(heartbeat(10, 1100)); // Counter did not progress.
    time = 499;
    engine.checkTimeouts();
    QVERIFY(engine.online());
    time = 500;
    engine.checkTimeouts();
    QVERIFY(!engine.online());
    QCOMPARE(counter(engine, "heartbeat_timeouts"), 1);
    engine.receiveFrame(heartbeat(11, 1100, true));
    QVERIFY(engine.online());
    QVERIFY(engine.led());
    QCOMPARE(engine.uptimeMs(), quint32(1100));
    QCOMPARE(engine.heartbeatCounter(), quint16(11));
    QCOMPARE(counter(engine, "valid_heartbeats"), 2);
}

void EngineTest::heartbeatFlagsAndFields()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    QCanBusFrame f = heartbeat(1, 100);
    f.setLocalEcho(true);
    engine.receiveFrame(f);
    f.setLocalEcho(false);
    f.setExtendedFrameFormat(true);
    engine.receiveFrame(f);
    f = heartbeat(1, 100);
    QByteArray p = f.payload(); p[0] = 2; f.setPayload(p);
    engine.receiveFrame(f);
    p[0] = 1; p[1] = 2; f.setPayload(p);
    engine.receiveFrame(f);
    QVERIFY(!engine.online());
    QCOMPARE(counter(engine, "valid_heartbeats"), 0);
    engine.receiveFrame(heartbeat(1, 100));
    QVERIFY(engine.online());
}

void EngineTest::rolloverIsNotRestart()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    engine.receiveFrame(heartbeat(65535, 1000));
    time = 100;
    engine.receiveFrame(heartbeat(0, 1100));
    QCOMPARE(engine.heartbeatCounter(), quint16(0));
    QCOMPARE(counter(engine, "restart_observations"), 0);
    engine.setConnected(false);
    engine.setConnected(true);
    engine.receiveFrame(heartbeat(100, 0xffffff9bu));
    time = 200;
    engine.receiveFrame(heartbeat(101, 0));
    QCOMPARE(engine.uptimeMs(), quint32(0));
    QCOMPARE(counter(engine, "restart_observations"), 0);
    QCOMPARE(counter(engine, "valid_heartbeats"), 4);
    engine.setConnected(false);
    engine.setConnected(true);
    engine.receiveFrame(heartbeat(65535, 0xffffff9bu));
    time = 300;
    engine.receiveFrame(heartbeat(0, 0));
    QCOMPARE(counter(engine, "restart_observations"), 0);
    QCOMPARE(counter(engine, "valid_heartbeats"), 6);
}

void EngineTest::staleHeartbeatSequenceDoesNotRestart()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    engine.receiveFrame(heartbeat(50, 5000));
    QVERIFY(engine.sendGetStatus());
    time = 300;
    engine.receiveFrame(heartbeat(48, 4800));
    time = 400;
    engine.receiveFrame(heartbeat(49, 4900));
    QCOMPARE(counter(engine, "restart_observations"), 0);
    QCOMPARE(counter(engine, "valid_heartbeats"), 1);
    QCOMPARE(engine.uptimeMs(), quint32(5000));
    QVERIFY(engine.pending());
    time = 500;
    engine.checkTimeouts();
    QVERIFY(!engine.online());
    QCOMPARE(counter(engine, "heartbeat_timeouts"), 1);
    QCOMPARE(lastOutcome(engine), QStringLiteral("timeout"));
    engine.receiveFrame(heartbeat(50, 5000));
    QVERIFY(!engine.online());
    time = 600;
    engine.receiveFrame(heartbeat(51, 5100));
    QVERIFY(engine.online());
    QCOMPARE(counter(engine, "restart_observations"), 0);
}

void EngineTest::conservativeRestartAndRecovery()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    engine.receiveFrame(heartbeat(50, 5000));
    QVERIFY(engine.sendSetLed(true));
    const quint16 oldSeq = engine.pendingSequence();
    time = 100;
    engine.receiveFrame(heartbeat(0, 0));
    QVERIFY(engine.pending());
    QCOMPARE(engine.uptimeMs(), quint32(5000));
    QCOMPARE(counter(engine, "restart_observations"), 0);
    time = 200;
    engine.receiveFrame(heartbeat(1, 100));
    QCOMPARE(counter(engine, "restart_observations"), 1);
    QCOMPARE(lastOutcome(engine), QStringLiteral("restart_observed"));
    QVERIFY(!engine.pending());
    engine.receiveFrame(response(oldSeq, 1, 0, true));
    QCOMPARE(counter(engine, "commands_succeeded"), 0);
    QVERIFY(engine.sendGetStatus());
    QVERIFY(engine.pendingSequence() != oldSeq);
    engine.receiveFrame(response(engine.pendingSequence()));
    QCOMPARE(lastOutcome(engine), QStringLiteral("success"));
    QVERIFY(!engine.report().value("hardware_verified").toBool());
}

void EngineTest::disconnectAndTransportFailure()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    connectEngine(engine);
    engine.receiveFrame(heartbeat(1, 100));
    QVERIFY(engine.sendGetStatus());
    engine.setConnected(false);
    QCOMPARE(lastOutcome(engine), QStringLiteral("disconnected"));
    QVERIFY(!engine.online());
    QVERIFY(!engine.pending());
    engine.receiveFrame(heartbeat(2, 200));
    QVERIFY(!engine.online());
    engine.setConnected(true);
    QVERIFY(!engine.online());
    engine.setSender([](const QCanBusFrame &, QString *error) {
        *error = QStringLiteral("Injected transport failure"); return false;
    });
    QVERIFY(!engine.sendGetStatus());
    QCOMPARE(lastOutcome(engine), QStringLiteral("transport_error"));
    QVERIFY(!engine.pending());
    QCOMPARE(counter(engine, "commands_sent"), 1);
    QCOMPARE(counter(engine, "transport_errors"), 1);
}

void EngineTest::boundedHistoryAndReports()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    engine.setSimulated(true);
    engine.setInterfaceName(QStringLiteral("vcan-unit-test"));
    QVERIFY2(engine.startLog(dir.filePath("frames.jsonl"), &error), qPrintable(error));
    connectEngine(engine);
    for (int i = 0; i < 300; ++i) {
        QVERIFY(engine.sendGetStatus());
        ++time;
        engine.receiveFrame(response(engine.pendingSequence()));
    }
    QCOMPARE(counter(engine, "commands_succeeded"), 300);
    QCOMPARE(counter(engine, "results_dropped"), 44);
    QCOMPARE(engine.report().value("command_results").toArray().size(), 256);
    QVERIFY2(engine.saveReport(dir.path(), &error), qPrintable(error));
    QFile report(dir.filePath("report.json"));
    QVERIFY(report.open(QIODevice::ReadOnly));
    QJsonParseError parseError;
    const QJsonObject summary = QJsonDocument::fromJson(report.readAll(), &parseError).object();
    QCOMPARE(parseError.error, QJsonParseError::NoError);
    QVERIFY(summary.value("simulated").toBool());
    QVERIFY(!summary.value("hardware_verified").toBool());
    QVERIFY(summary.value("log_healthy").toBool());
    QCOMPARE(summary.value("counters_exact_decimal").toObject().value("commands_succeeded").toString(), QStringLiteral("300"));
    QFile markdown(dir.filePath("report.md"));
    QVERIFY(markdown.open(QIODevice::ReadOnly));
    QVERIFY(markdown.readAll().contains("Hardware verified: **false**"));
    QFile log(dir.filePath("frames.jsonl"));
    QVERIFY(log.open(QIODevice::ReadOnly));
    int tx = 0, rx = 0, results = 0;
    while (!log.atEnd()) {
        const QJsonObject entry = QJsonDocument::fromJson(log.readLine(), &parseError).object();
        QCOMPARE(parseError.error, QJsonParseError::NoError);
        QVERIFY(entry.contains("utc"));
        QVERIFY(entry.contains("monotonic_ms"));
        if (entry.value("direction").toString() == "tx_attempt") ++tx;
        if (entry.value("direction").toString() == "rx") ++rx;
        if (entry.value("kind").toString() == "command_result") ++results;
    }
    QCOMPARE(tx, 300);
    QCOMPARE(rx, 300);
    QCOMPARE(results, 300);
}

void EngineTest::failedLogIsVisible()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QSignalSpy events(&engine, &DiagnosticEngine::event);
    QVERIFY(!engine.startLog(dir.filePath("missing/frames.jsonl"), &error));
    QVERIFY(!error.isEmpty());
    QCOMPARE(counter(engine, "log_errors"), 1);
    QVERIFY(!engine.report().value("log_healthy").toBool());
    QCOMPARE(events.last().at(0).toString(), QStringLiteral("log_error"));
    QVERIFY(!engine.saveReport(dir.path(), &error));
    QVERIFY(error.contains("session log failed"));
    QVERIFY(QFile::exists(dir.filePath("report.json")));
#ifdef Q_OS_UNIX
    if (QFile::exists(QStringLiteral("/dev/full"))) {
        DiagnosticEngine full(nullptr, [&] { return time; });
        QVERIFY(!full.startLog(QStringLiteral("/dev/full"), &error));
        QCOMPARE(counter(full, "log_errors"), 1);
    }
#endif
}


void EngineTest::submissionStopsWhenLoggingFails_data()
{
    QTest::addColumn<bool>("kernelWriteFailure");
    QTest::newRow("log-open-failure-in-pending-callback") << false;
#ifdef Q_OS_LINUX
    QTest::newRow("actual-write-enospc-before-transport") << true;
#endif
}

void EngineTest::submissionStopsWhenLoggingFails()
{
    QFETCH(bool, kernelWriteFailure);
    DiagnosticEngine engine;
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath("frames.jsonl");
    QString error;
    QVERIFY(engine.startLog(path, &error));
    engine.setConnected(true);
    int submissions = 0;
    bool injected = false;
    bool injectionWorked = false;
    engine.setSender([&](const QCanBusFrame &, QString *) { ++submissions; return true; });
    QSignalSpy completed(&engine, &DiagnosticEngine::commandFinished);
    connect(&engine, &DiagnosticEngine::pendingChanged, &engine, [&](bool pending) {
        if (!pending || injected) return;
        injected = true;
        if (!kernelWriteFailure) {
            QString why;
            injectionWorked = !engine.startLog(dir.path(), &why);
            return;
        }
#ifdef Q_OS_LINUX
        // Replace only this test's own log descriptor. QFile's next write/flush
        // then receives real ENOSPC; no resource limit, mount or CAN device changes.
        const auto descriptors = QDir("/proc/self/fd").entryList(QDir::Files | QDir::System | QDir::NoDotAndDotDot);
        for (const auto &name : descriptors) {
            const QFileInfo info("/proc/self/fd/" + name);
            if (info.symLinkTarget() != path) continue;
            bool ok = false;
            const int logFd = name.toInt(&ok);
            const int fullFd = ::open("/dev/full", O_WRONLY);
            if (ok && fullFd >= 0) {
                injectionWorked = ::dup2(fullFd, logFd) == logFd;
                ::close(fullFd);
            }
            break;
        }
#endif
    });
    const bool accepted = engine.sendSetLed(true);
    QVERIFY(injected);
    QVERIFY2(injectionWorked, "The test must induce a real log failure before asserting behaviour");
    QCOMPARE(counter(engine, "log_errors"), 1);
    QCOMPARE(submissions, 0);
    QVERIFY(!accepted);
    QVERIFY(!engine.pending());
    QCOMPARE(completed.size(), 1);
    QCOMPARE(lastOutcome(engine), QStringLiteral("log_error"));
    QCOMPARE(counter(engine, "commands_attempted"), 1);
    QCOMPARE(counter(engine, "commands_failed"), 1);
    QCOMPARE(counter(engine, "commands_sent"), 0);
    QCOMPARE(counter(engine, "transport_errors"), 0);
}

void EngineTest::failedSessionRejectsNewSubmission()
{
    DiagnosticEngine engine;
    QTemporaryDir dir;
    QString error;
    QVERIFY(!engine.startLog(dir.path(), &error));
    int submissions = 0;
    engine.setSender([&](const QCanBusFrame &, QString *) { ++submissions; return true; });
    engine.setConnected(true);
    QVERIFY(!engine.sendGetStatus());
    QCOMPARE(submissions, 0);
    QVERIFY(!engine.pending());
    QCOMPARE(counter(engine, "commands_rejected"), 1);
    QCOMPARE(counter(engine, "commands_attempted"), 0);
}

void EngineTest::submissionCancelledByDisconnect()
{
    DiagnosticEngine engine;
    int submissions = 0;
    engine.setSender([&](const QCanBusFrame &, QString *) { ++submissions; return true; });
    engine.setConnected(true);
    QSignalSpy completed(&engine, &DiagnosticEngine::commandFinished);
    connect(&engine, &DiagnosticEngine::pendingChanged, &engine, [&](bool pending) {
        if (pending) engine.setConnected(false);
    });
    QVERIFY(!engine.sendGetStatus());
    QCOMPARE(submissions, 0);
    QCOMPARE(completed.size(), 1);
    QCOMPARE(lastOutcome(engine), QStringLiteral("disconnected"));
}


void EngineTest::pendingCallbackReplacesRequest()
{
    DiagnosticEngine engine;
    QList<QCanBusFrame> submitted;
    engine.setSender([&](const QCanBusFrame &frame, QString *) { submitted.append(frame); return true; });
    engine.setConnected(true);
    bool replaced = false;
    bool nestedAccepted = false;
    connect(&engine, &DiagnosticEngine::pendingChanged, &engine, [&](bool pending) {
        if (!pending || replaced) return;
        replaced = true;
        engine.setConnected(false);
        engine.setConnected(true);
        nestedAccepted = engine.sendGetStatus();
    });
    QVERIFY(!engine.sendSetLed(true));
    QVERIFY(nestedAccepted);
    QCOMPARE(submitted.size(), 1);
    QCOMPARE(submitted.first().payload(), QByteArray::fromHex("0102020000000000"));
    QVERIFY(engine.pending());
    engine.receiveFrame(response(1, 1, 0, true));
    QVERIFY(engine.pending());
    engine.receiveFrame(response(2));
    QVERIFY(!engine.pending());
    QCOMPARE(counter(engine, "commands_succeeded"), 1);
    QCOMPARE(counter(engine, "commands_failed"), 1);
}

void EngineTest::senderRemovedBeforeSubmission()
{
    DiagnosticEngine engine;
    connectEngine(engine);
    connect(&engine, &DiagnosticEngine::pendingChanged, &engine, [&](bool pending) {
        if (pending) engine.setSender({});
    });
    QVERIFY(!engine.sendGetStatus());
    QVERIFY(!engine.pending());
    QCOMPARE(lastOutcome(engine), QStringLiteral("transport_error"));
    QCOMPARE(counter(engine, "commands_sent"), 0);
}

void EngineTest::noLogSynchronousReplyStillWorks()
{
    DiagnosticEngine engine;
    engine.setConnected(true);
    int submissions = 0;
    engine.setSender([&](const QCanBusFrame &frame, QString *) {
        ++submissions;
        const auto p = frame.payload();
        const quint16 seq = quint8(p[2]) | (quint16(quint8(p[3])) << 8);
        engine.receiveFrame(response(seq));
        return true;
    });
    QVERIFY(engine.sendGetStatus());
    QCOMPARE(submissions, 1);
    QCOMPARE(lastOutcome(engine), QStringLiteral("success"));
    QVERIFY(!engine.pending());
    QCOMPARE(counter(engine, "commands_sent"), 1);
}

void EngineTest::loggingFailureAfterSubmissionDoesNotUndoTransport()
{
    qint64 time = 0;
    DiagnosticEngine engine(nullptr, [&] { return time; });
    QTemporaryDir dir;
    QString error;
    QVERIFY(engine.startLog(dir.filePath("frames.jsonl"), &error));
    engine.setConnected(true);
    int submissions = 0;
    engine.setSender([&](const QCanBusFrame &, QString *) {
        ++submissions;
        QString why;
        engine.startLog(dir.path(), &why); // The transport has already accepted the frame.
        return true;
    });
    QVERIFY(engine.sendGetStatus());
    QCOMPARE(submissions, 1);
    QCOMPARE(counter(engine, "commands_sent"), 1);
    QVERIFY(engine.pending());
    time = 500;
    engine.checkTimeouts();
    QCOMPARE(lastOutcome(engine), QStringLiteral("timeout"));
    QCOMPARE(submissions, 1);
}

QTEST_GUILESS_MAIN(EngineTest)
#include "test_engine.moc"
