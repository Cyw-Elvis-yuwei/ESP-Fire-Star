#ifndef CANBENCH_RECOVERYRUN_H
#define CANBENCH_RECOVERYRUN_H
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <functional>
class DiagnosticEngine;

// Same-adapter node outage/recovery check. Does not fabricate or inject a fault.
class RecoveryRun : public QObject {
    Q_OBJECT
public:
    explicit RecoveryRun(DiagnosticEngine *engine, QObject *parent = nullptr,
                         std::function<qint64()> clock = {});
    bool start(const QString &newDirectory, QString *error);
    void cancel();
    bool running() const { return m_running; }
    QString phase() const { return m_phase; }
    QString directory() const { return m_directory; }
    QJsonObject report() const;
signals:
    void progress(QString text);
    void phaseChanged(QString phase);
    void finished(bool passed, QString text);
private slots:
    void poll();
private:
    qint64 now() const;
    void transition(const QString &phase, const QString &text);
    void completed(quint16 seq, quint8 op, bool success, QString outcome, qint64 latency);
    void finish(const QString &outcome);
    bool writeJson(QString *error);
    DiagnosticEngine *m_engine;
    std::function<qint64()> m_clock;
    QElapsedTimer m_elapsed;
    QTimer m_timer;
    bool m_running=false, m_waiting=false, m_saved=false;
    quint16 m_sequence=0;
    qint64 m_started=0, m_phaseAt=0, m_ended=0;
    QString m_phase="idle", m_outcome="idle", m_directory, m_error;
    QJsonObject m_metadata;
    QJsonArray m_events, m_commands;
};
#endif
