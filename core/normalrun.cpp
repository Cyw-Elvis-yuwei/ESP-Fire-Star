#include "normalrun.h"
#include "diagnosticengine.h"
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <QVariant>

namespace {
const QStringList Names = {QStringLiteral("读取初始状态"), QStringLiteral("关闭LED"),
    QStringLiteral("查询并确认关闭"), QStringLiteral("打开LED"),
    QStringLiteral("查询并确认开启"), QStringLiteral("恢复初始状态"),
    QStringLiteral("查询并确认已恢复")};
const int Operations[] = {2, 1, 2, 1, 2, 1, 2};
QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool writeFile(const QString &path, const QByteArray &bytes, QString *error) {
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit()) {
        *error = path + ": " + file.errorString();
        return false;
    }
    return true;
}
}

NormalRun::NormalRun(DiagnosticEngine *engine, QObject *parent) : QObject(parent), m_engine(engine)
{
    m_next.setSingleShot(true);
    connect(&m_next, &QTimer::timeout, this, &NormalRun::advance);
    connect(engine, &DiagnosticEngine::pendingChanged, this, [this](bool pending) {
        if (m_running && m_waiting && pending) m_sequence = m_engine->pendingSequence();
    });
    connect(engine, &DiagnosticEngine::commandFinished, this, &NormalRun::commandFinished);
    connect(engine, &DiagnosticEngine::statusChanged, this, [this](bool connected, bool online) {
        if (m_running && (!connected || !online)) finish(connected ? "heartbeat_timeout" : "disconnected");
    });
    connect(engine, &DiagnosticEngine::event, this, [this](const QString &name, const QString &) {
        if (m_running && (name == "transport_error" || name == "log_error" || name == "restart_observed"))
            finish(name);
    });
}

bool NormalRun::start(const QString &newDirectory, QString *error)
{
    error->clear();
    m_engine->checkTimeouts();
    if (m_running || m_engine->pending() || !m_engine->connected() || !m_engine->online()) {
        *error = QStringLiteral("请等待设备在线且当前命令结束后再开始。");
        return false;
    }
    if (!m_engine->report().value("log_healthy").toBool()) {
        *error = QStringLiteral("原始报文日志不可用，不能开始回归。");
        return false;
    }
    // Reserve a new directory; never mix a previous run with this run.
    if (QDir(newDirectory).exists() || !QDir().mkdir(newDirectory)) {
        *error = QStringLiteral("请使用可写且尚不存在的报告目录。");
        return false;
    }
    m_directory = newDirectory;
    m_index = 0;
    m_waiting = false;
    m_steps = QJsonArray();
    for (int i = 0; i < Names.size(); ++i)
        m_steps.append(QJsonObject{{"name", Names.at(i)}, {"op", Operations[i]}, {"outcome", "not_run"}});
    const auto source = m_engine->report();
    m_report = QJsonObject{{"schema_version", 1}, {"suite", "normal-control-v1"},
        {"started_utc", utc()}, {"interface", source.value("interface")},
        {"simulated", source.value("simulated")}, {"protocol_passed", false},
        {"physical_led_observation", "not_observed_by_software"},
        {"raw_log", source.value("log_path")}, {"start_monotonic_ms", source.value("monotonic_ms")},
        {"start_counters", m_engine->counters()}, {"outcome", "running"}, {"steps", m_steps}};
    if (!writeFile(QDir(m_directory).filePath("result.json"), QJsonDocument(m_report).toJson(), error))
        return false; // Prove report storage is writable before touching the LED.
    m_running = true;
    m_next.start(0);
    emit progress(QStringLiteral("开始正常回归：7步，结束后恢复初始LED状态。"));
    return true;
}

void NormalRun::advance()
{
    if (!m_running) return;
    m_engine->checkTimeouts();
    if (!m_running) return;
    if (!m_engine->online() || !m_engine->connected() || m_engine->pending()) {
        finish("device_not_ready");
        return;
    }
    m_waiting = true;
    QJsonObject step = m_steps.at(m_index).toObject();
    step["outcome"] = "pending";
    if (m_index > 0) step["expected_led"] = m_index >= 5 ? m_initialLed : m_index >= 3;
    m_steps[m_index] = step;
    emit progress(QStringLiteral("正在执行 %1/7：%2").arg(m_index + 1).arg(Names.at(m_index)));
    const bool accepted = Operations[m_index] == 1
        ? m_engine->sendSetLed(step.value("expected_led").toBool()) : m_engine->sendGetStatus();
    if (!accepted && m_running) finish("send_rejected");
}

void NormalRun::commandFinished(quint16 seq, quint8 op, bool success, QString outcome, qint64 latency)
{
    if (!m_running || !m_waiting) return;
    if (seq != m_sequence || op != Operations[m_index]) { finish("unexpected_command"); return; }
    QJsonObject step = m_steps.at(m_index).toObject();
    if (success && m_index > 0 && m_engine->led() != step.value("expected_led").toBool()) {
        success = false;
        outcome = "query_state_mismatch";
    }
    step["seq"] = int(seq);
    step["success"] = success;
    step["outcome"] = outcome;
    step["latency_ms"] = double(latency);
    step["finished_utc"] = utc();
    if (success) step["observed_led"] = m_engine->led();
    m_steps[m_index] = step;
    m_waiting = false;
    if (!success) { finish(outcome); return; }
    if (m_index == 0) { m_initialLed = m_engine->led(); m_report["initial_led"] = m_initialLed; }
    ++m_index;
    if (m_index == Names.size()) { finish("passed"); return; }
    emit progress(QStringLiteral("已通过 %1/7，下一步：%2").arg(m_index).arg(Names.at(m_index)));
    m_next.start(400); // Human-visible switching; one command in flight, no load generation.
}

void NormalRun::cancel() { if (m_running) finish("cancelled"); }

void NormalRun::finish(const QString &outcome)
{
    if (!m_running) return;
    m_running = false;
    m_next.stop();
    if (m_waiting) {
        auto step = m_steps.at(m_index).toObject();
        step["seq"] = int(m_sequence);
        step["success"] = false;
        step["outcome"] = outcome;
        m_steps[m_index] = step;
    }
    m_waiting = false;
    const bool passed = outcome == "passed" && m_index == Names.size();
    m_report["protocol_passed"] = passed;
    m_report["outcome"] = outcome;
    m_report["steps"] = m_steps;
    m_report["finished_utc"] = utc();
    m_report["end_monotonic_ms"] = m_engine->report().value("monotonic_ms");
    m_report["end_counters"] = m_engine->counters();
    m_report["initial_state_restored"] = passed;
    m_report["note"] = QStringLiteral("只验证本轮协议控制与查询。实物灯需人工观察；不包含负载、断线恢复、复位或升级。失败/停止后不继续发恢复命令，LED状态请重新查询。");
    QString error;
    const bool saved = save(&error);
    m_report["report_saved"] = saved;
    if (!saved) m_report["report_error"] = error;
    emit finished(passed && saved, !saved ? QStringLiteral("报告保存失败：") + error
        : (passed ? QStringLiteral("正常回归通过 7/7，已恢复初始状态。报告：")
                  : QStringLiteral("正常回归未通过：%1；后续步骤停止。报告：").arg(outcome)) + m_directory);
}

bool NormalRun::save(QString *error)
{
    QString md = QStringLiteral("# CAN 正常回归报告\n\n接口：%1；模拟：%2\n\n协议结果：**%3**\n\n%4\n\n"
        "| 步骤 | 结果 | 序号 | 耗时ms |\n|---|---|---:|---:|\n")
        .arg(m_report.value("interface").toString())
        .arg(m_report.value("simulated").toBool() ? "true" : "false")
        .arg(m_report.value("outcome").toString()).arg(m_report.value("note").toString());
    for (const auto &v : m_steps) {
        const auto s = v.toObject();
        md += QStringLiteral("| %1 | %2 | %3 | %4 |\n").arg(s.value("name").toString())
            .arg(s.value("outcome").toString()).arg(s.value("seq").toVariant().toString())
            .arg(s.value("latency_ms").toVariant().toString());
    }
    md += QStringLiteral("\n原始报文：%1\n\n起止UTC：%2 ～ %3\n")
        .arg(m_report.value("raw_log").toString()).arg(m_report.value("started_utc").toString())
        .arg(m_report.value("finished_utc").toString());
    if (!writeFile(QDir(m_directory).filePath("result.md"), md.toUtf8(), error)) return false;
    m_report["report_saved"] = true;
    return writeFile(QDir(m_directory).filePath("result.json"), QJsonDocument(m_report).toJson(), error);
}
