#include "continuousrun.h"
#include "diagnosticengine.h"
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <algorithm>
#include <numeric>
#include <utility>

namespace {
const int IntervalMs = 100;
QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool writeFile(const QString &path, const QByteArray &bytes, QString *error) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly) || f.write(bytes) != bytes.size() || !f.commit()) {
        *error = path + ": " + f.errorString(); return false;
    }
    return true;
}
}

ContinuousRun::ContinuousRun(DiagnosticEngine *engine, QObject *parent, std::function<qint64()> clock)
    : QObject(parent), m_engine(engine), m_clock(std::move(clock))
{
    m_elapsed.start();
    m_timer.setTimerType(Qt::PreciseTimer);
    m_timer.setInterval(20);
    connect(&m_timer, &QTimer::timeout, this, &ContinuousRun::poll);
    connect(engine, &DiagnosticEngine::pendingChanged, this, [this](bool pending) {
        if (m_running && m_waiting && pending) m_sequence = m_engine->pendingSequence();
    });
    connect(engine, &DiagnosticEngine::commandFinished, this, &ContinuousRun::completed);
    connect(engine, &DiagnosticEngine::statusChanged, this, [this](bool connected, bool online) {
        if (m_running && (!connected || !online)) requestStop(connected ? "heartbeat_timeout" : "disconnected");
    });
    connect(engine, &DiagnosticEngine::event, this, [this](const QString &name, const QString &) {
        if (m_running && (name == "transport_error" || name == "log_error" || name == "restart_observed"))
            requestStop(name);
    });
}

qint64 ContinuousRun::now() const { return m_clock ? m_clock() : m_elapsed.elapsed(); }

bool ContinuousRun::start(const QString &directory, QString *error, int durationMs)
{
    error->clear();
    m_engine->checkTimeouts();
    if (m_running || m_engine->pending() || !m_engine->connected() || !m_engine->online()) {
        *error = QStringLiteral("请等待设备在线且当前命令结束。"); return false;
    }
    if (durationMs < 200 || durationMs > 60000) {
        *error = QStringLiteral("测试时长须在200～60000ms内。"); return false;
    }
    const auto source = m_engine->report();
    if (!source.value("log_healthy").toBool()) {
        *error = QStringLiteral("原始报文日志不可用。"); return false;
    }
    if (QDir(directory).exists() || !QDir().mkdir(directory)) {
        *error = QStringLiteral("报告目录须可写且尚不存在。"); return false;
    }
    m_directory = directory; m_duration = durationMs;
    m_attempts = m_successes = m_failures = m_timeouts = 0;
    m_waiting = false; m_reportSaved = false; m_stopReason.clear(); m_reportError.clear();
    m_outcome = "running"; m_samples = QJsonArray(); m_latencies.clear();
    m_started = m_ended = m_nextRequest = m_lastProgress = now();
    m_metadata = QJsonObject{{"schema_version", 1}, {"suite", "continuous-status-v1"},
        {"interface", source.value("interface")}, {"simulated", source.value("simulated")},
        {"started_utc", utc()}, {"start_monotonic_ms", source.value("monotonic_ms")},
        {"raw_log", source.value("log_path")}, {"start_counters", m_engine->counters()},
        {"operation", "GET_STATUS only"}, {"interval_ms", IntervalMs}, {"command_timeout_ms", 500},
        {"note", QStringLiteral("最多每秒10次查询，单请求在途。响应延迟为主机端观测；不代表总线饱和压力测试或长期可靠性。提前停止不算完整通过，不改变LED、不复位。")}};
    if (!writeFile(QDir(directory).filePath("result.json"), QJsonDocument(report()).toJson(), error))
        return false;
    m_running = true;
    m_timer.start();
    showProgress();
    return true;
}

void ContinuousRun::poll()
{
    if (!m_running) return;
    m_engine->checkTimeouts();
    if (!m_running) return;
    if (!m_stopReason.isEmpty()) {
        if (!m_waiting) finish(m_stopReason);
        return;
    }
    if (now() - m_started >= m_duration) {
        if (!m_waiting) finish(m_failures == 0 && m_successes > 0 ? "passed" : "completed_with_failures");
        return;
    }
    if (now() - m_lastProgress >= 1000) showProgress();
    if (m_waiting || now() < m_nextRequest) return;
    if (!m_engine->online() || !m_engine->connected() || m_engine->pending()) {
        requestStop("device_not_ready"); return;
    }
    m_nextRequest = now() + IntervalMs; // Never catch up with a burst after a slow reply.
    m_waiting = true;
    m_sequence = 0;
    ++m_attempts;
    const bool accepted = m_engine->sendGetStatus();
    // Transport errors normally finish synchronously in the engine. Handle a bare rejection too.
    if (!accepted && m_running && m_waiting) {
        completed(m_sequence, 2, false, "send_rejected", 0);
        requestStop("send_rejected");
    }
}

void ContinuousRun::completed(quint16 seq, quint8 op, bool success, QString outcome, qint64 latency)
{
    if (!m_running || !m_waiting) return;
    if (seq != m_sequence || op != 2) { success = false; outcome = "unexpected_command"; }
    m_waiting = false;
    if (success) { ++m_successes; m_latencies.append(latency); }
    else { ++m_failures; if (outcome == "timeout") ++m_timeouts; }
    m_samples.append(QJsonObject{{"seq", int(seq)}, {"op", int(op)}, {"success", success},
        {"outcome", outcome}, {"latency_ms", double(latency)}, {"finished_utc", utc()},
        {"elapsed_ms", double(now() - m_started)}});
    if (outcome == "transport_error" || outcome == "disconnected" || outcome == "restart_observed"
        || outcome == "unexpected_command" || outcome == "send_rejected") requestStop(outcome);
    if (m_running && !m_stopReason.isEmpty()) finish(m_stopReason);
}

void ContinuousRun::cancel() { requestStop("cancelled"); }

void ContinuousRun::requestStop(const QString &reason)
{
    if (!m_running) return;
    if (m_stopReason.isEmpty()) m_stopReason = reason;
    if (!m_waiting) finish(m_stopReason);
    else emit progress(QStringLiteral("正在结束测试，等待当前请求完成或超时（最多500ms）：") + m_stopReason);
}

QJsonObject ContinuousRun::report() const
{
    auto r = m_metadata;
    const qint64 elapsed = qMax(qint64(0), (m_running ? now() : m_ended) - m_started);
    r["requested_duration_ms"] = m_duration;
    r["elapsed_ms"] = double(elapsed);
    r["outcome"] = m_outcome;
    r["protocol_passed"] = m_outcome == "passed";
    r["full_duration_completed"] = !m_running && elapsed >= m_duration && m_stopReason.isEmpty();
    r["attempts"] = m_attempts; r["succeeded"] = m_successes;
    r["failed"] = m_failures; r["timeouts"] = m_timeouts;
    r["pending_requests"] = m_waiting ? 1 : 0;
    r["success_rate_percent"] = m_attempts ? QJsonValue(100.0 * m_successes / m_attempts) : QJsonValue();
    r["actual_queries_per_second"] = elapsed > 0 ? QJsonValue(1000.0 * m_attempts / elapsed) : QJsonValue();
    r["samples"] = m_samples;
    r["latency_population"] = "successful replies only; P95 nearest-rank";
    if (!m_latencies.isEmpty()) {
        auto values = m_latencies; std::sort(values.begin(), values.end());
        const int rank = (95 * values.size() + 99) / 100 - 1;
        r["latency_ms"] = QJsonObject{{"min", double(values.first())}, {"max", double(values.last())},
            {"mean", std::accumulate(values.begin(), values.end(), 0.0) / values.size()},
            {"p95", double(values.at(rank))}, {"count", values.size()}};
    } else r["latency_ms"] = QJsonValue();
    r["report_saved"] = m_reportSaved;
    if (!m_reportError.isEmpty()) r["report_error"] = m_reportError;
    return r;
}

void ContinuousRun::showProgress()
{
    m_lastProgress = now();
    emit progress(QStringLiteral("持续查询 %1/%2秒 · 成功 %3/%4 · 失败 %5 · 超时 %6")
        .arg(qMin(m_duration, int(now() - m_started)) / 1000).arg(m_duration / 1000)
        .arg(m_successes).arg(m_attempts).arg(m_failures).arg(m_timeouts));
}

void ContinuousRun::finish(const QString &outcome)
{
    if (!m_running) return;
    m_running = false; m_timer.stop(); m_ended = now(); m_outcome = outcome;
    m_metadata["finished_utc"] = utc();
    m_metadata["end_monotonic_ms"] = m_engine->report().value("monotonic_ms");
    m_metadata["end_counters"] = m_engine->counters();
    QString error;
    const bool saved = save(&error);
    m_reportSaved = saved;
    if (!saved) m_reportError = error;
    emit finished(outcome == "passed" && saved, !saved ? QStringLiteral("持续测试报告保存失败：") + error
        : QStringLiteral("持续测试%1 · 成功 %2/%3，超时 %4。报告：%5")
            .arg(outcome == "passed" ? QStringLiteral("通过") : QStringLiteral("未通过（%1）").arg(outcome))
            .arg(m_successes).arg(m_attempts).arg(m_timeouts).arg(m_directory));
}

bool ContinuousRun::save(QString *error)
{
    const auto r = report();
    const auto latency = r.value("latency_ms").toObject();
    QString md = QStringLiteral("# CAN 持续通信测试\n\n接口：%1；模拟：%2\n\n结果：**%3**\n\n"
        "计划时长：%4ms；实际：%5ms；发送间隔至少100ms，单请求在途。\n\n"
        "成功 %6/%7；失败 %8；超时 %9；成功率 %10%。\n\n")
        .arg(r.value("interface").toString()).arg(r.value("simulated").toBool() ? "true" : "false")
        .arg(m_outcome).arg(m_duration).arg(m_ended-m_started).arg(m_successes).arg(m_attempts)
        .arg(m_failures).arg(m_timeouts).arg(m_attempts ? QString::number(100.0*m_successes/m_attempts, 'f', 2) : "n/a");
    if (!latency.isEmpty()) md += QStringLiteral("成功回复延迟：最小 %1ms；平均 %2ms；P95 %3ms；最大 %4ms。\n\n")
        .arg(latency.value("min").toDouble()).arg(latency.value("mean").toDouble(), 0, 'f', 2)
        .arg(latency.value("p95").toDouble()).arg(latency.value("max").toDouble());
    md += QStringLiteral("%1\n\n完整逐条结果见result.json；原始报文：%2\n\n起止UTC：%3 ～ %4\n")
        .arg(m_metadata.value("note").toString()).arg(m_metadata.value("raw_log").toString())
        .arg(m_metadata.value("started_utc").toString()).arg(m_metadata.value("finished_utc").toString());
    if (!writeFile(QDir(m_directory).filePath("result.md"), md.toUtf8(), error)) return false;
    m_reportSaved = true;
    return writeFile(QDir(m_directory).filePath("result.json"), QJsonDocument(report()).toJson(), error);
}
