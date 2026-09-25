#ifndef CANBENCH_CONTINUOUSRUN_H
#define CANBENCH_CONTINUOUSRUN_H
#include <QObject>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonArray>
#include <QJsonObject>
#include <QVector>
#include <functional>
class DiagnosticEngine;

// Bounded, read-only status queries. Matching and deadlines stay in DiagnosticEngine.
class ContinuousRun : public QObject {
    Q_OBJECT
public:
    explicit ContinuousRun(DiagnosticEngine *engine, QObject *parent = nullptr,
                           std::function<qint64()> clock = {});
    bool start(const QString &newDirectory, QString *error, int durationMs = 60000);
    void cancel();
    bool running() const { return m_running; }
    QString directory() const { return m_directory; }
    QJsonObject report() const;
signals:
    void progress(QString text);
    void finished(bool passed, QString text);
private slots:
    void poll();
private:
    qint64 now() const;
    void completed(quint16 seq, quint8 op, bool success, QString outcome, qint64 latency);
    void requestStop(const QString &reason);
    void finish(const QString &outcome);
    void showProgress();
    bool save(QString *error);
    DiagnosticEngine *m_engine;
    QTimer m_timer;
    QElapsedTimer m_elapsed;
    std::function<qint64()> m_clock;
    bool m_running = false, m_waiting = false, m_reportSaved = false;
    quint16 m_sequence = 0;
    int m_duration = 60000, m_attempts = 0, m_successes = 0, m_failures = 0, m_timeouts = 0;
    qint64 m_started = 0, m_ended = 0, m_nextRequest = 0, m_lastProgress = 0;
    QString m_directory, m_stopReason, m_outcome = "idle", m_reportError;
    QJsonObject m_metadata;
    QJsonArray m_samples;
    QVector<qint64> m_latencies;
};
#endif
