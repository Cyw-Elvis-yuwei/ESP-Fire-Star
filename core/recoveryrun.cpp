#include "recoveryrun.h"
#include "diagnosticengine.h"
#include <QDateTime>
#include <QDir>
#include <QJsonDocument>
#include <QSaveFile>
#include <utility>
namespace {
QString utc() { return QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs); }
bool writeFile(const QString &path,const QByteArray &bytes,QString *error) {
    QSaveFile f(path);
    if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit()) {
        *error=path+": "+f.errorString();return false;
    }
    return true;
}
}
RecoveryRun::RecoveryRun(DiagnosticEngine *engine,QObject *parent,std::function<qint64()> clock)
    :QObject(parent),m_engine(engine),m_clock(std::move(clock))
{
    m_elapsed.start();m_timer.setInterval(25);
    connect(&m_timer,&QTimer::timeout,this,&RecoveryRun::poll);
    connect(engine,&DiagnosticEngine::pendingChanged,this,[this](bool pending) {
        if(m_running&&m_waiting&&pending)m_sequence=m_engine->pendingSequence();
    });
    connect(engine,&DiagnosticEngine::commandFinished,this,&RecoveryRun::completed);
    connect(engine,&DiagnosticEngine::statusChanged,this,[this](bool connected,bool online) {
        if(!m_running)return;
        if(!connected){finish("adapter_disconnected_not_covered");return;}
        if(!online&&m_phase=="waiting_for_outage")
            transition("probing_outage",QStringLiteral("已检测设备离线，正在验证查询超时。"));
        if(online&&m_phase=="waiting_for_recovery")
            transition("verifying_recovery",QStringLiteral("心跳已恢复，正在发送新查询确认通信。"));
    });
    connect(engine,&DiagnosticEngine::event,this,[this](const QString &name,const QString &detail) {
        if(!m_running)return;
        if(name=="log_error"||name=="transport_error"){finish(name);return;}
        if(name=="restart_observed") {
            m_events.append(QJsonObject{{"event",name},{"detail",detail},{"elapsed_ms",double(now()-m_started)},{"utc",utc()}});
        }
    });
}
qint64 RecoveryRun::now() const {return m_clock?m_clock():m_elapsed.elapsed();}

bool RecoveryRun::start(const QString &directory,QString *error)
{
    error->clear();m_engine->checkTimeouts();
    if(m_running||m_engine->pending()||!m_engine->connected()||!m_engine->online()) {
        *error=QStringLiteral("请等待设备在线且当前命令结束。");return false;
    }
    const auto source=m_engine->report();
    if(!source.value("log_healthy").toBool()){*error=QStringLiteral("原始报文日志不可用。");return false;}
    if(QDir(directory).exists()||!QDir().mkdir(directory)){*error=QStringLiteral("报告目录须可写且尚不存在。");return false;}
    m_directory=directory;m_started=m_ended=m_phaseAt=now();m_waiting=false;m_saved=false;m_error.clear();
    m_events=QJsonArray();m_commands=QJsonArray();m_phase="baseline_query";m_outcome="running";
    m_metadata=QJsonObject{{"schema_version",1},{"suite","node-recovery-v1"},{"interface",source.value("interface")},
        {"simulated",source.value("simulated")},{"started_utc",utc()},{"start_monotonic_ms",source.value("monotonic_ms")},
        {"start_counters",m_engine->counters()},{"raw_log",source.value("log_path")},
        {"fault_injection","external; see separate evidence"},{"physical_unplug_verified",false},
        {"note",QStringLiteral("保持CAN适配器及接口存在。检查节点心跳消失、一次查询超时，再以恢复后的新查询确认通信。故障原因与恢复动作须另附证据；不等于USB拔插自动重连。")}};
    if(!writeJson(error))return false;
    m_running=true;m_timer.start();
    transition("baseline_query",QStringLiteral("先查询正常状态，再等待节点停止响应；CANable保持连接。"));
    return m_running;
}

void RecoveryRun::transition(const QString &phase,const QString &text)
{
    if(!m_running)return;
    m_phase=phase;m_phaseAt=now();
    m_events.append(QJsonObject{{"event",phase},{"elapsed_ms",double(now()-m_started)},{"utc",utc()}});
    QString error;
    if(!writeJson(&error)){m_error=error;finish("report_error");return;}
    emit progress(text);emit phaseChanged(phase);
}

void RecoveryRun::poll()
{
    if(!m_running)return;
    m_engine->checkTimeouts();if(!m_running)return;
    if(now()-m_phaseAt>=60000){finish("stage_deadline");return;}
    if(m_waiting)return;
    if(m_phase=="waiting_for_recovery"&&m_engine->online())
        transition("verifying_recovery",QStringLiteral("心跳已恢复，正在发送新查询确认通信。"));
    if(!m_running)return;
    if(m_phase!="baseline_query"&&m_phase!="probing_outage"&&m_phase!="verifying_recovery")return;
    if(m_phase!="probing_outage"&&!m_engine->online()){finish("device_offline_before_query");return;}
    m_waiting=true;m_sequence=0;
    const bool accepted=m_engine->sendGetStatus();
    if(!accepted&&m_running)finish("send_rejected");
}

void RecoveryRun::completed(quint16 seq,quint8 op,bool success,QString outcome,qint64 latency)
{
    if(!m_running||!m_waiting)return;
    m_waiting=false;
    if(seq!=m_sequence||op!=2){finish("unexpected_command");return;}
    m_commands.append(QJsonObject{{"phase",m_phase},{"seq",int(seq)},{"op",int(op)},
        {"success",success},{"outcome",outcome},{"latency_ms",double(latency)},
        {"elapsed_ms",double(now()-m_started)},{"utc",utc()}});
    if(m_phase=="baseline_query") {
        if(!success){finish("baseline_failed");return;}
        transition("waiting_for_outage",QStringLiteral("正常查询通过。等待节点停止响应，最长60秒；CANable保持连接。"));
    } else if(m_phase=="probing_outage") {
        if(success||outcome!="timeout"){finish("expected_timeout_not_observed");return;}
        transition("waiting_for_recovery",QStringLiteral("离线及查询超时已确认。等待节点恢复（必要时复位开发板），最长60秒。"));
    } else if(m_phase=="verifying_recovery") {
        if(!success){finish("recovery_query_failed");return;}
        if(!m_engine->online()){finish("heartbeat_not_fresh");return;}
        finish("passed");
    }
}
void RecoveryRun::cancel(){if(m_running)finish("cancelled");}
QJsonObject RecoveryRun::report() const
{
    auto r=m_metadata;r["phase"]=m_phase;r["outcome"]=m_outcome;
    r["protocol_passed"]=m_outcome=="passed";r["report_saved"]=m_saved;
    r["elapsed_ms"]=double((m_running?now():m_ended)-m_started);
    r["events"]=m_events;r["commands"]=m_commands;
    if(!m_error.isEmpty())r["report_error"]=m_error;
    return r;
}
bool RecoveryRun::writeJson(QString *error)
{
    return writeFile(QDir(m_directory).filePath("result.json"),QJsonDocument(report()).toJson(),error);
}
void RecoveryRun::finish(const QString &outcome)
{
    if(!m_running)return;
    m_running=false;m_timer.stop();m_ended=now();m_outcome=outcome;
    m_metadata["finished_utc"]=utc();m_metadata["end_monotonic_ms"]=m_engine->report().value("monotonic_ms");
    m_metadata["end_counters"]=m_engine->counters();m_metadata["pending_at_end"]=m_engine->pending();
    QString md=QStringLiteral("# 节点故障恢复检查\n\n结果：**%1**；接口：%2；模拟：%3\n\n%4\n\n"
        "| 阶段 | 序号 | 结果 | 响应ms |\n|---|---:|---|---:|\n")
        .arg(outcome).arg(m_metadata.value("interface").toString()).arg(m_metadata.value("simulated").toBool()?"true":"false")
        .arg(m_metadata.value("note").toString());
    for(const auto &v:m_commands){const auto c=v.toObject();md+=QStringLiteral("| %1 | %2 | %3 | %4 |\n")
        .arg(c.value("phase").toString()).arg(c.value("seq").toInt()).arg(c.value("outcome").toString()).arg(c.value("latency_ms").toDouble());}
    md+=QStringLiteral("\n起止UTC：%1 ～ %2\n\n原始报文：%3\n").arg(m_metadata.value("started_utc").toString())
        .arg(m_metadata.value("finished_utc").toString()).arg(m_metadata.value("raw_log").toString());
    QString error;
    m_saved=writeFile(QDir(m_directory).filePath("result.md"),md.toUtf8(),&error);
    if(!writeJson(&error))m_saved=false;
    if(!m_saved)m_error=error;
    emit finished(outcome=="passed"&&m_saved,!m_saved?QStringLiteral("恢复检查报告保存失败：")+error
        :QStringLiteral("节点恢复检查%1。报告：%2").arg(outcome=="passed"?QStringLiteral("通过：离线、超时及恢复后新查询均已确认")
            :QStringLiteral("未通过（%1）").arg(outcome)).arg(m_directory));
}
