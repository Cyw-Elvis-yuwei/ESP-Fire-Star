#include "diagnosticengine.h"

#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QStringList>
#include <utility>

namespace {
const qint64 TimeoutMs = 500;
const int ResultLimit = 256;
quint8 byte(const QByteArray &p, int i) { return quint8(p.at(i)); }
quint16 u16(const QByteArray &p, int i)
{
    return quint16(byte(p, i)) | (quint16(byte(p, i + 1)) << 8);
}
quint32 u32(const QByteArray &p, int i)
{
    return quint32(byte(p, i)) | (quint32(byte(p, i + 1)) << 8)
            | (quint32(byte(p, i + 2)) << 16) | (quint32(byte(p, i + 3)) << 24);
}
bool forward16(quint16 previous, quint16 current)
{
    const quint16 delta = quint16(current - previous);
    return delta != 0 && delta < 0x8000;
}
bool forward32(quint32 previous, quint32 current)
{
    const quint32 delta = current - previous;
    return delta != 0 && delta < 0x80000000u;
}
QString utcNow() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
}

DiagnosticEngine::DiagnosticEngine(QObject *parent, std::function<qint64()> clock)
    : QObject(parent), m_clock(std::move(clock)), m_startedUtc(utcNow())
{
    m_elapsed.start();
    const QStringList names = {QStringLiteral("rx_frames"), QStringLiteral("tx_frames"),
        QStringLiteral("commands_attempted"), QStringLiteral("commands_sent"),
        QStringLiteral("commands_succeeded"), QStringLiteral("commands_failed"),
        QStringLiteral("commands_timeouts"), QStringLiteral("commands_rejected"),
        QStringLiteral("invalid_frames"), QStringLiteral("ignored_frames"),
        QStringLiteral("local_echo_frames"), QStringLiteral("unmatched_responses"),
        QStringLiteral("valid_heartbeats"), QStringLiteral("duplicate_heartbeats"),
        QStringLiteral("stale_heartbeats"), QStringLiteral("heartbeat_timeouts"),
        QStringLiteral("restart_observations"), QStringLiteral("transport_errors"),
        QStringLiteral("log_errors"), QStringLiteral("results_dropped")};
    for (const QString &name : names) m_counters.insert(name, 0);
    m_timer.setInterval(25);
    connect(&m_timer, &QTimer::timeout, this, &DiagnosticEngine::checkTimeouts);
    m_timer.start();
}

qint64 DiagnosticEngine::now() const { return m_clock ? m_clock() : m_elapsed.elapsed(); }

void DiagnosticEngine::setSender(std::function<bool(const QCanBusFrame &, QString *)> sender)
{
    m_sender = std::move(sender);
}

void DiagnosticEngine::setSimulated(bool simulated) { m_simulated = simulated; }
void DiagnosticEngine::setInterfaceName(QString name) { m_interfaceName = std::move(name); }

void DiagnosticEngine::setConnected(bool connected)
{
    if (m_connected == connected) return;
    m_connected = connected;
    m_online = false;
    m_haveHeartbeat = false;
    m_restartCandidate = false;
    if (!connected && m_pending) finishCommand(false, QStringLiteral("disconnected"), now());
    emitEvent(connected ? QStringLiteral("connected") : QStringLiteral("disconnected"),
              m_interfaceName);
    emit statusChanged(m_connected, m_online);
}

void DiagnosticEngine::count(const QString &name)
{
    ++m_counters[name];
    emit countersChanged();
}

void DiagnosticEngine::emitEvent(const QString &name, const QString &detail)
{
    writeLog(QJsonObject{{QStringLiteral("kind"), QStringLiteral("event")},
                        {QStringLiteral("name"), name}, {QStringLiteral("detail"), detail}});
    emit event(name, detail);
}

bool DiagnosticEngine::startLog(QString filePath, QString *error)
{
    if (error) error->clear();
    m_log.close();
    m_logRequested = true;
    m_logPath = filePath;
    m_logError.clear();
    m_log.setFileName(filePath);
    if (!m_log.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        failLog(m_log.errorString());
        if (error) *error = m_logError;
        return false;
    }
    emitEvent(QStringLiteral("log_started"), filePath);
    if (!m_logError.isEmpty()) {
        if (error) *error = m_logError;
        return false;
    }
    return true;
}

void DiagnosticEngine::failLog(const QString &detail)
{
    if (!m_logError.isEmpty()) return;
    m_logError = detail.isEmpty() ? QStringLiteral("Log write failed") : detail;
    m_log.close();
    count(QStringLiteral("log_errors"));
    emit event(QStringLiteral("log_error"), m_logError);
}

bool DiagnosticEngine::writeLog(QJsonObject entry)
{
    if (!m_logRequested) return true;
    if (!m_logError.isEmpty()) return false;
    entry.insert(QStringLiteral("utc"), utcNow());
    entry.insert(QStringLiteral("monotonic_ms"), double(now()));
    const QByteArray line = QJsonDocument(entry).toJson(QJsonDocument::Compact) + '\n';
    if (!m_log.isOpen() || m_log.write(line) != line.size() || !m_log.flush()) {
        failLog(m_log.errorString());
        return false;
    }
    return true;
}

void DiagnosticEngine::logFrame(const QString &direction, const QCanBusFrame &frame)
{
    writeLog(QJsonObject{{QStringLiteral("kind"), QStringLiteral("frame")},
        {QStringLiteral("direction"), direction},
        {QStringLiteral("id"), double(frame.frameId())},
        {QStringLiteral("payload_hex"), QString::fromLatin1(frame.payload().toHex())},
        {QStringLiteral("dlc"), frame.payload().size()},
        {QStringLiteral("type"), int(frame.frameType())},
        {QStringLiteral("extended"), frame.hasExtendedFrameFormat()},
        {QStringLiteral("fd"), frame.hasFlexibleDataRateFormat()},
        {QStringLiteral("brs"), frame.hasBitrateSwitch()},
        {QStringLiteral("esi"), frame.hasErrorStateIndicator()},
        {QStringLiteral("local_echo"), frame.hasLocalEcho()}});
}

void DiagnosticEngine::recordTransportError(QString error)
{
    count(QStringLiteral("transport_errors"));
    emitEvent(QStringLiteral("transport_error"), error);
    if (m_pending) finishCommand(false, QStringLiteral("transport_error"), now());
}

bool DiagnosticEngine::sendSetLed(bool on) { return sendCommand(1, on); }
bool DiagnosticEngine::sendGetStatus() { return sendCommand(2, false); }

bool DiagnosticEngine::sendCommand(quint8 operation, bool desiredLed)
{
    checkTimeouts();
    if (!m_connected || m_pending || !m_sender) {
        count(QStringLiteral("commands_rejected"));
        emitEvent(QStringLiteral("command_rejected"), !m_connected ? QStringLiteral("disconnected")
                  : m_pending ? QStringLiteral("busy") : QStringLiteral("sender_missing"));
        return false;
    }
    m_pending = true;
    m_pendingSequence = m_nextSequence++;
    m_pendingOperation = operation;
    m_desiredLed = desiredLed;
    m_commandAt = now();
    QByteArray payload(8, '\0');
    payload[0] = 1;
    payload[1] = char(operation);
    payload[2] = char(m_pendingSequence & 0xff);
    payload[3] = char(m_pendingSequence >> 8);
    payload[4] = operation == 1 && desiredLed ? 1 : 0;
    const QCanBusFrame frame(0x321, payload);
    count(QStringLiteral("commands_attempted"));
    emit pendingChanged(true);
    logFrame(QStringLiteral("tx_attempt"), frame);
    QString error;
    if (!m_sender(frame, &error)) {
        recordTransportError(error.isEmpty() ? QStringLiteral("Sender rejected frame") : error);
        return false;
    }
    count(QStringLiteral("tx_frames"));
    count(QStringLiteral("commands_sent"));
    emitEvent(QStringLiteral("tx_accepted"), QStringLiteral("seq=%1 op=%2")
              .arg(u16(payload, 2)).arg(operation));
    return true;
}

void DiagnosticEngine::finishCommand(bool success, const QString &outcome, qint64 at)
{
    if (!m_pending) return;
    const quint16 seq = m_pendingSequence;
    const quint8 op = m_pendingOperation;
    const qint64 latency = qMax(qint64(0), at - m_commandAt);
    m_pending = false;
    count(success ? QStringLiteral("commands_succeeded") : QStringLiteral("commands_failed"));
    if (outcome == QStringLiteral("timeout")) count(QStringLiteral("commands_timeouts"));
    const QJsonObject result{{QStringLiteral("seq"), int(seq)}, {QStringLiteral("op"), int(op)},
        {QStringLiteral("success"), success}, {QStringLiteral("outcome"), outcome},
        {QStringLiteral("latency_ms"), double(latency)}, {QStringLiteral("utc"), utcNow()},
        {QStringLiteral("monotonic_ms"), double(at)}};
    if (m_results.size() >= ResultLimit) {
        m_results.removeAt(0);
        count(QStringLiteral("results_dropped"));
    }
    m_results.append(result);
    QJsonObject logged = result;
    logged.insert(QStringLiteral("kind"), QStringLiteral("command_result"));
    writeLog(logged);
    emit pendingChanged(false);
    emit commandFinished(seq, op, success, outcome, latency);
}

void DiagnosticEngine::checkTimeouts()
{
    const qint64 at = now();
    if (m_pending && at - m_commandAt >= TimeoutMs)
        finishCommand(false, QStringLiteral("timeout"), at);
    if (m_online && at - m_lastHeartbeatAt >= TimeoutMs) {
        m_online = false;
        count(QStringLiteral("heartbeat_timeouts"));
        emitEvent(QStringLiteral("heartbeat_timeout"), QStringLiteral("No fresh heartbeat for 500 ms"));
        emit statusChanged(m_connected, m_online);
    }
    if (m_restartCandidate && at - m_candidateAt >= TimeoutMs) m_restartCandidate = false;
}

void DiagnosticEngine::rejectFrame(const QString &reason)
{
    count(QStringLiteral("invalid_frames"));
    emitEvent(QStringLiteral("invalid_frame"), reason);
}

void DiagnosticEngine::receiveFrame(QCanBusFrame frame)
{
    count(QStringLiteral("rx_frames"));
    logFrame(QStringLiteral("rx"), frame);
    checkTimeouts(); // An overdue response must not win a race with a delayed timer callback.
    if (frame.hasLocalEcho()) {
        count(QStringLiteral("local_echo_frames"));
        return;
    }
    if (!m_connected) { count(QStringLiteral("ignored_frames")); return; }
    if (frame.frameId() != 0x123 && frame.frameId() != 0x322) {
        count(QStringLiteral("ignored_frames"));
        return;
    }
    if (frame.frameType() != QCanBusFrame::DataFrame || frame.hasExtendedFrameFormat()
            || frame.hasFlexibleDataRateFormat() || frame.hasBitrateSwitch()
            || frame.hasErrorStateIndicator() || frame.payload().size() != 8 || !frame.isValid()) {
        rejectFrame(QStringLiteral("Expected standard classical CAN data frame with DLC 8"));
        return;
    }
    const QByteArray payload = frame.payload();
    if (byte(payload, 0) != 1) { rejectFrame(QStringLiteral("Unsupported version")); return; }
    if (frame.frameId() == 0x123) receiveHeartbeat(payload, now());
    else receiveResponse(payload, now());
}

void DiagnosticEngine::receiveResponse(const QByteArray &payload, qint64 at)
{
    const quint8 op = byte(payload, 1);
    const quint16 seq = u16(payload, 2);
    const quint8 result = byte(payload, 4);
    if ((op != 1 && op != 2) || result > 3 || byte(payload, 5) > 1
            || byte(payload, 6) != 0 || byte(payload, 7) != 0) {
        rejectFrame(QStringLiteral("Invalid response operation, result, LED or reserved fields"));
        return;
    }
    if (!m_pending || seq != m_pendingSequence || op != m_pendingOperation) {
        count(QStringLiteral("unmatched_responses"));
        emitEvent(QStringLiteral("unmatched_response"), QStringLiteral("seq=%1 op=%2").arg(seq).arg(op));
        return;
    }
    if (at - m_commandAt >= TimeoutMs) {
        finishCommand(false, QStringLiteral("timeout"), at);
        count(QStringLiteral("unmatched_responses"));
        return;
    }
    if (result != 0) {
        finishCommand(false, QStringLiteral("response_error_%1").arg(result), at);
        return;
    }
    const bool led = byte(payload, 5) == 1;
    if (op == 1 && led != m_desiredLed) {
        finishCommand(false, QStringLiteral("state_mismatch"), at);
        return;
    }
    m_led = led;
    emit telemetryChanged(m_led, m_uptime, m_heartbeatCounter);
    finishCommand(true, QStringLiteral("success"), at);
}

void DiagnosticEngine::acceptHeartbeat(bool led, quint16 counter, quint32 uptime, qint64 at)
{
    const bool wasOnline = m_online;
    m_haveHeartbeat = true;
    m_online = true;
    m_led = led;
    m_heartbeatCounter = counter;
    m_uptime = uptime;
    m_lastHeartbeatAt = at;
    m_restartCandidate = false;
    count(QStringLiteral("valid_heartbeats"));
    emit telemetryChanged(m_led, m_uptime, m_heartbeatCounter);
    if (!wasOnline) {
        emitEvent(QStringLiteral("heartbeat_online"), QStringLiteral("Fresh protocol heartbeat observed"));
        emit statusChanged(m_connected, m_online);
    }
}

void DiagnosticEngine::receiveHeartbeat(const QByteArray &payload, qint64 at)
{
    if (byte(payload, 1) > 1) { rejectFrame(QStringLiteral("Invalid heartbeat LED")); return; }
    const bool led = byte(payload, 1) == 1;
    const quint16 counter = u16(payload, 2);
    const quint32 uptime = u32(payload, 4);
    if (!m_haveHeartbeat) { acceptHeartbeat(led, counter, uptime, at); return; }
    if (counter == m_heartbeatCounter && uptime == m_uptime) {
        count(QStringLiteral("duplicate_heartbeats"));
        return;
    }
    if (forward16(m_heartbeatCounter, counter) && forward32(m_uptime, uptime)) {
        acceptHeartbeat(led, counter, uptime, at);
        return;
    }
    // Only a clear early-boot reset is a candidate. Two ordinary stale heartbeats
    // must not refresh liveness or cancel a command. CAN v1 has no boot identifier:
    // replayed historical startup frames remain indistinguishable from a reset,
    // and a restart outside this conservative observation window may be missed.
    // Counter reset can resemble a 16-bit rollover, so exclude natural uptime
    // rollover separately. This remains an observation, not hardware-reset proof.
    const bool startupSample = uptime <= 500 && counter <= 5;
    const bool resetBranch = startupSample && uptime < m_uptime && counter < m_heartbeatCounter
            && m_uptime - uptime >= 500 && !forward32(m_uptime, uptime);
    if (m_restartCandidate && startupSample && uptime < m_uptime && at - m_candidateAt < TimeoutMs
            && forward16(m_candidateCounter, counter) && forward32(m_candidateUptime, uptime)) {
        count(QStringLiteral("restart_observations"));
        if (m_pending) finishCommand(false, QStringLiteral("restart_observed"), at);
        emitEvent(QStringLiteral("restart_observed"),
                  QStringLiteral("Two progressing reset samples; hardware reset and command recovery unverified"));
        acceptHeartbeat(led, counter, uptime, at);
        return;
    }
    if (resetBranch) {
        m_restartCandidate = true;
        m_candidateCounter = counter;
        m_candidateUptime = uptime;
        m_candidateAt = at;
    }
    count(QStringLiteral("stale_heartbeats"));
}

QJsonObject DiagnosticEngine::counters() const
{
    QJsonObject result;
    for (auto it = m_counters.cbegin(); it != m_counters.cend(); ++it)
        result.insert(it.key(), double(it.value()));
    return result;
}

QJsonObject DiagnosticEngine::report() const
{
    QJsonObject exactCounters;
    for (auto it = m_counters.cbegin(); it != m_counters.cend(); ++it)
        exactCounters.insert(it.key(), QString::number(it.value()));
    return QJsonObject{{QStringLiteral("schema_version"), 1},
        {QStringLiteral("protocol_version"), 1}, {QStringLiteral("started_utc"), m_startedUtc},
        {QStringLiteral("generated_utc"), utcNow()}, {QStringLiteral("monotonic_ms"), double(now())},
        {QStringLiteral("simulated"), m_simulated}, {QStringLiteral("hardware_verified"), false},
        {QStringLiteral("interface"), m_interfaceName}, {QStringLiteral("connected"), m_connected},
        {QStringLiteral("online"), m_online}, {QStringLiteral("led"), m_led},
        {QStringLiteral("uptime_ms"), double(m_uptime)},
        {QStringLiteral("heartbeat_counter"), int(m_heartbeatCounter)},
        {QStringLiteral("pending"), m_pending}, {QStringLiteral("counters"), counters()},
        {QStringLiteral("counters_exact_decimal"), exactCounters},
        {QStringLiteral("command_results"), m_results},
        {QStringLiteral("result_history_limit"), ResultLimit},
        {QStringLiteral("log_requested"), m_logRequested},
        {QStringLiteral("log_healthy"), m_logRequested && m_log.isOpen() && m_logError.isEmpty()},
        {QStringLiteral("log_path"), m_logPath}, {QStringLiteral("log_error"), m_logError},
        {QStringLiteral("evidence_note"), QStringLiteral("Software protocol observations only. Virtual CAN does not verify physical CAN, STM32 firmware, wiring or the physical LED.")}};
}

bool DiagnosticEngine::saveReport(QString directory, QString *error)
{
    if (error) error->clear();
    if (!QDir().mkpath(directory)) {
        if (error) *error = QStringLiteral("Cannot create report directory: ") + directory;
        emitEvent(QStringLiteral("report_error"), QStringLiteral("Cannot create directory: ") + directory);
        return false;
    }
    const QJsonObject summary = report();
    QString markdown = QStringLiteral("# CAN diagnostic session\n\n"
        "- Simulated: %1\n- Hardware verified: **false**\n- Interface: %2\n"
        "- Connected: %3\n- Online (fresh heartbeat): %4\n- Log healthy: %5\n"
        "- Log error: %6\n\nSoftware protocol observations only. Virtual CAN does not verify "
        "physical CAN, STM32 firmware, wiring or the physical LED. Restart events are "
        "observations from counters and uptime; a new matching command is needed to demonstrate recovery.\n\n"
        "## Counters\n\n| Counter | Total |\n| --- | ---: |\n")
        .arg(m_simulated ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_interfaceName).arg(m_connected ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_online ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(summary.value(QStringLiteral("log_healthy")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
        .arg(m_logError);
    for (auto it = m_counters.cbegin(); it != m_counters.cend(); ++it)
        markdown += QStringLiteral("| %1 | %2 |\n").arg(it.key()).arg(it.value());
    markdown += QStringLiteral("\n## Recent command results\n\nAt most %1 results retained; counters cover the whole session.\n\n"
        "| Sequence | Operation | Success | Outcome | Latency ms |\n| ---: | ---: | --- | --- | ---: |\n").arg(ResultLimit);
    for (const QJsonValue &value : m_results) {
        const QJsonObject r = value.toObject();
        markdown += QStringLiteral("| %1 | %2 | %3 | %4 | %5 |\n")
            .arg(r.value(QStringLiteral("seq")).toInt()).arg(r.value(QStringLiteral("op")).toInt())
            .arg(r.value(QStringLiteral("success")).toBool() ? QStringLiteral("true") : QStringLiteral("false"))
            .arg(r.value(QStringLiteral("outcome")).toString()).arg(r.value(QStringLiteral("latency_ms")).toDouble(), 0, 'f', 0);
    }
    const auto save = [&directory, error](const QString &name, const QByteArray &data) {
        QSaveFile file(QDir(directory).filePath(name));
        if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size() || !file.commit()) {
            if (error) *error = name + QStringLiteral(": ") + file.errorString();
            return false;
        }
        return true;
    };
    if (!save(QStringLiteral("report.json"), QJsonDocument(summary).toJson(QJsonDocument::Indented))
            || !save(QStringLiteral("report.md"), markdown.toUtf8())) {
        emitEvent(QStringLiteral("report_error"), error ? *error : QStringLiteral("Report write failed"));
        return false;
    }
    if (m_logRequested && !m_logError.isEmpty()) {
        if (error) *error = QStringLiteral("Reports saved, but session log failed: ") + m_logError;
        return false;
    }
    return true;
}
