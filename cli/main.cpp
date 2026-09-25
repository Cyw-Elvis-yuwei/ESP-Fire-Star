#include "diagnosticengine.h"
#include <QCanBus>
#include <QCanBusDevice>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QProcess>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <memory>

static bool writeJson(const QString &path, const QJsonObject &value)
{
    QSaveFile file(path);
    const QByteArray bytes = QJsonDocument(value).toJson(QJsonDocument::Indented);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size() && file.commit();
}

static bool verifyVcan(const QString &name, QString *error)
{
    if (!QRegularExpression("^[A-Za-z0-9_.:-]{1,15}$").match(name).hasMatch()) {
        *error = "Invalid interface name";
        return false;
    }
    QProcess ip;
    ip.start("ip", {"-j", "-d", "link", "show", "dev", name});
    if (!ip.waitForStarted(1000) || !ip.waitForFinished(3000)) {
        ip.kill();
        ip.waitForFinished(1000);
        *error = "Could not verify interface using ip -j -d link";
        return false;
    }
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(ip.readAllStandardOutput(), &parseError);
    if (ip.exitStatus() != QProcess::NormalExit || ip.exitCode() != 0 ||
            parseError.error != QJsonParseError::NoError || !doc.isArray() ||
            doc.array().size() != 1 ||
            doc.array().at(0).toObject().value("linkinfo").toObject().value("info_kind").toString() != "vcan") {
        *error = "Regression refuses an interface that is not verified info_kind=vcan";
        return false;
    }
    return true;
}

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    app.setApplicationName("canbench-cli");
    QCommandLineParser parser;
    parser.setApplicationDescription("Simulated protocol regression on verified Linux vcan only.");
    parser.addHelpOption();
    parser.addOption({"interface", "Verified vcan interface", "name", "vcan0"});
    parser.addOption({"output", "New directory for result, raw log and engine reports", "directory", "artifacts/cli"});
    parser.addOption({"case", "normal|response-timeout|heartbeat-timeout|restart|wrong-seq|late-response", "name", "normal"});
    parser.addOption({"duration-ms", "Observation duration (2300..30000 ms)", "milliseconds", "3000"});
    if (!parser.parse(app.arguments())) {
        QTextStream(stderr) << parser.errorText() << '\n';
        return 2;
    }
    if (parser.isSet("help")) {
        QTextStream(stdout) << parser.helpText();
        return 0;
    }
    const QString output = QDir(parser.value("output")).absolutePath();
    const QString caseName = parser.value("case");
    const QString interfaceName = parser.value("interface");
    bool durationOk = false;
    const int duration = parser.value("duration-ms").toInt(&durationOk);
    const QStringList cases = {"normal", "response-timeout", "heartbeat-timeout", "restart", "wrong-seq", "late-response"};
    const auto configError = [&](const QString &detail) {
        QTextStream(stderr) << "Configuration error: " << detail << '\n';
        return 2;
    };
    if (!cases.contains(caseName) || !durationOk || duration < 2300 || duration > 30000 || !parser.positionalArguments().isEmpty())
        return configError("Invalid case, duration or positional arguments");
    if (QFile::exists(QDir(output).filePath("result.json")))
        return configError("Output already contains result.json; choose a new directory");
    if (!QDir().mkpath(output))
        return configError("Cannot create output directory");
    QString error;
    const auto recordConfigError = [&](const QString &detail) {
        QJsonObject result;
        result["case"] = caseName;
        result["pass"] = false;
        result["simulated"] = true;
        result["hardware_verified"] = false;
        result["configuration_error"] = detail;
        writeJson(QDir(output).filePath("result.json"), result);
        return configError(detail);
    };
    if (!verifyVcan(interfaceName, &error))
        return recordConfigError(error);
    std::unique_ptr<QCanBusDevice> device(QCanBus::instance()->createDevice("socketcan", interfaceName, &error));
    if (!device)
        return recordConfigError(error);
    DiagnosticEngine engine;
    engine.setSimulated(true);
    engine.setInterfaceName(interfaceName);
    if (!engine.startLog(QDir(output).filePath("raw.jsonl"), &error))
        return recordConfigError(error);
    engine.setSender([&](const QCanBusFrame &frame, QString *sendError) {
        const bool written = device->writeFrame(frame);
        if (!written && sendError)
            *sendError = device->errorString();
        return written;
    });
    QObject::connect(device.get(), &QCanBusDevice::framesReceived, &app, [&]() {
        while (device->framesAvailable())
            engine.receiveFrame(device->readFrame());
    });
    QObject::connect(device.get(), &QCanBusDevice::errorOccurred, &app, [&](QCanBusDevice::CanBusError code) {
        if (code != QCanBusDevice::NoError)
            engine.recordTransportError(device->errorString());
    });
    QObject::connect(device.get(), &QCanBusDevice::stateChanged, &app, [&](QCanBusDevice::CanBusDeviceState state) {
        engine.setConnected(state == QCanBusDevice::ConnectedState);
    });
    if (!device->connectDevice())
        return recordConfigError(device->errorString());
    engine.setConnected(device->state() == QCanBusDevice::ConnectedState);
    QElapsedTimer elapsed;
    elapsed.start();
    QJsonArray observed, events;
    QString commandLabel;
    bool sendFailure = false;
    qint64 restartAt = -1, offlineAt = -1;
    QObject::connect(&engine, &DiagnosticEngine::event, &app, [&](QString name, QString detail) {
        QJsonObject entry;
        entry["name"] = name;
        entry["detail"] = detail;
        entry["at_ms"] = double(elapsed.elapsed());
        events.append(entry);
        if (name == "restart_observed")
            restartAt = elapsed.elapsed();
    });
    QObject::connect(&engine, &DiagnosticEngine::statusChanged, &app, [&](bool, bool online) {
        if (!online && engine.counters().value("heartbeat_timeouts").toInt() > 0)
            offlineAt = elapsed.elapsed();
    });
    QObject::connect(&engine, &DiagnosticEngine::commandFinished, &app,
                     [&](quint16 seq, quint8 op, bool success, QString outcome, qint64 latency) {
        QJsonObject entry;
        entry["label"] = commandLabel;
        entry["seq"] = int(seq);
        entry["op"] = int(op);
        entry["success"] = success;
        entry["outcome"] = outcome;
        entry["latency_ms"] = double(latency);
        entry["at_ms"] = double(elapsed.elapsed());
        entry["led"] = engine.led();
        entry["online"] = engine.online();
        observed.append(entry);
        QTextStream(stdout) << commandLabel << " seq=" << seq << " " << outcome << " " << latency << " ms\n";
    });
    const auto schedule = [&](int at, const QString &label, int op, bool led) {
        QTimer::singleShot(at, &app, [&, label, op, led]() {
            if (engine.pending()) {
                sendFailure = true;
                return;
            }
            commandLabel = label;
            if (!(op == 1 ? engine.sendSetLed(led) : engine.sendGetStatus()))
                sendFailure = true;
        });
    };
    schedule(150, "initial_set_on", 1, true);
    if (caseName == "normal") {
        schedule(450, "query_on", 2, false);
        schedule(750, "set_off", 1, false);
        schedule(1050, "query_off", 2, false);
    } else if (caseName == "heartbeat-timeout") {
        schedule(1200, "query_while_offline", 2, false);
        schedule(1900, "recovery_query", 2, false);
    } else {
        schedule(caseName == "restart" ? 1600 : caseName == "late-response" ? 750 : 1050,
                 "recovery_query", 2, false);
    }
    int exitCode = 1;
    QTimer::singleShot(duration, &app, [&]() {
        QJsonArray assertions;
        bool passed = true;
        const auto check = [&](const QString &name, bool ok) {
            QJsonObject item;
            item["name"] = name;
            item["pass"] = ok;
            assertions.append(item);
            passed = passed && ok;
        };
        const auto command = [&](const QString &label) {
            for (const QJsonValue &value : observed) {
                if (value.toObject().value("label").toString() == label)
                    return value.toObject();
            }
            return QJsonObject();
        };
        const auto successful = [&](const QString &label, bool led) {
            const QJsonObject value = command(label);
            return value.value("success").toBool() && value.value("outcome").toString() == "success" &&
                    value.value("led").toBool() == led;
        };
        const QJsonObject counters = engine.counters();
        const bool responseFault = caseName == "response-timeout" || caseName == "wrong-seq" || caseName == "late-response";
        const int expectedCommands = caseName == "normal" ? 4 : caseName == "heartbeat-timeout" ? 3 : 2;
        check("all_scheduled_commands_completed", !sendFailure && observed.size() == expectedCommands && !engine.pending());
        check("device_connected", engine.connected());
        check("fresh_heartbeats_observed", counters.value("valid_heartbeats").toInt() >= 5);
        check("online_at_end", engine.online());
        check("no_transport_errors", counters.value("transport_errors").toInt() == 0);
        check("no_invalid_frames", counters.value("invalid_frames").toInt() == 0);
        check("expected_heartbeat_timeout_count", counters.value("heartbeat_timeouts").toInt() == (caseName == "heartbeat-timeout" ? 1 : 0));
        check("expected_restart_count", counters.value("restart_observations").toInt() == (caseName == "restart" ? 1 : 0));
        check("expected_response_timeout_count", counters.value("commands_timeouts").toInt(-1) == (responseFault ? 1 : 0));
        if (responseFault) {
            const QJsonObject initial = command("initial_set_on");
            check("initial_request_detected_timeout", !initial.isEmpty() && !initial.value("success").toBool() &&
                  initial.value("outcome").toString() == "timeout" && initial.value("latency_ms").toDouble() >= 500);
            check("fresh_query_recovered_after_timeout", successful("recovery_query", true) &&
                  command("recovery_query").value("seq").toInt() != initial.value("seq").toInt() &&
                  command("recovery_query").value("at_ms").toDouble() > initial.value("at_ms").toDouble());
            if (caseName != "response-timeout")
                check("mismatched_or_late_reply_rejected", counters.value("unmatched_responses").toInt() >= 1);
            if (caseName == "late-response")
                check("recovery_waited_for_own_reply", command("recovery_query").value("latency_ms").toDouble() >= 300);
        } else {
            check("initial_set_succeeded", successful("initial_set_on", true));
            if (caseName == "normal") {
                check("query_confirms_on", successful("query_on", true));
                check("set_off_succeeded", successful("set_off", false));
                check("query_confirms_off", successful("query_off", false));
            } else if (caseName == "heartbeat-timeout") {
                check("reply_during_heartbeat_outage_does_not_mark_online", successful("query_while_offline", true) &&
                      !command("query_while_offline").value("online").toBool() && offlineAt >= 0);
                check("fresh_query_after_heartbeat_recovery", successful("recovery_query", true) &&
                      command("recovery_query").value("online").toBool() &&
                      command("recovery_query").value("at_ms").toDouble() > offlineAt);
            } else {
                check("fresh_query_after_restart_confirms_reset_led", successful("recovery_query", false) &&
                      restartAt >= 0 && command("recovery_query").value("at_ms").toDouble() > restartAt);
            }
        }
        QString reportError;
        check("engine_report_saved", engine.saveReport(QDir(output).filePath("engine"), &reportError));
        check("raw_log_has_no_errors", engine.counters().value("log_errors").toInt() == 0);
        QJsonObject result;
        result["case"] = caseName;
        result["pass"] = passed;
        result["simulated"] = true;
        result["hardware_verified"] = false;
        result["interface"] = interfaceName;
        result["duration_ms"] = double(elapsed.elapsed());
        result["assertions"] = assertions;
        result["commands"] = observed;
        result["events"] = events;
        result["engine_report"] = "engine/report.json";
        if (!reportError.isEmpty())
            result["report_error"] = reportError;
        if (!writeJson(QDir(output).filePath("result.json"), result)) {
            passed = false;
            QTextStream(stderr) << "Failed to save result.json\n";
        }
        exitCode = passed ? 0 : 1;
        QTextStream(stdout) << (passed ? "PASS " : "FAIL ") << caseName << '\n';
        app.quit();
    });
    app.exec();
    device->disconnectDevice();
    return exitCode;
}
