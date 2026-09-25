#ifndef DIAGNOSTICENGINE_H
#define DIAGNOSTICENGINE_H

#include <QObject>
#include <QCanBusFrame>
#include <QElapsedTimer>
#include <QFile>
#include <QJsonArray>
#include <QJsonObject>
#include <QMap>
#include <QTimer>
#include <functional>

class DiagnosticEngine : public QObject
{
    Q_OBJECT
public:
    explicit DiagnosticEngine(QObject *parent = nullptr,
                              std::function<qint64()> clock = {});
    void setSender(std::function<bool(const QCanBusFrame &, QString *)> sender);
    void setConnected(bool connected);
    void setSimulated(bool simulated);
    void setInterfaceName(QString name);
    void receiveFrame(QCanBusFrame frame);
    bool sendSetLed(bool on);
    bool sendGetStatus();
    bool startLog(QString filePath, QString *error = nullptr);
    QJsonObject report() const;
    bool saveReport(QString directory, QString *error = nullptr);
    void recordTransportError(QString error);

    bool connected() const { return m_connected; }
    bool online() const { return m_online; }
    bool led() const { return m_led; }
    quint32 uptimeMs() const { return m_uptime; }
    quint16 heartbeatCounter() const { return m_heartbeatCounter; }
    bool pending() const { return m_pending; }
    quint16 pendingSequence() const { return m_pendingSequence; }
    QJsonObject counters() const;

public slots:
    void checkTimeouts();

signals:
    void statusChanged(bool connected, bool online);
    void telemetryChanged(bool led, quint32 uptime, quint16 counter);
    void pendingChanged(bool pending);
    void commandFinished(quint16 seq, quint8 op, bool success,
                         QString outcome, qint64 latencyMs);
    void event(QString name, QString detail);
    void countersChanged();

private:
    qint64 now() const;
    bool sendCommand(quint8 operation, bool desiredLed);
    void finishCommand(bool success, const QString &outcome, qint64 at);
    void receiveHeartbeat(const QByteArray &payload, qint64 at);
    void receiveResponse(const QByteArray &payload, qint64 at);
    void count(const QString &name);
    void emitEvent(const QString &name, const QString &detail);
    void logFrame(const QString &direction, const QCanBusFrame &frame);
    bool writeLog(QJsonObject entry);
    void failLog(const QString &detail);
    void rejectFrame(const QString &reason);
    void acceptHeartbeat(bool led, quint16 counter, quint32 uptime, qint64 at);

    QElapsedTimer m_elapsed;
    std::function<qint64()> m_clock;
    std::function<bool(const QCanBusFrame &, QString *)> m_sender;
    QTimer m_timer;
    QFile m_log;
    QString m_logPath;
    QString m_logError;
    QString m_interfaceName;
    QString m_startedUtc;
    bool m_logRequested = false;
    bool m_simulated = true;
    bool m_connected = false;
    bool m_online = false;
    bool m_led = false;
    bool m_haveHeartbeat = false;
    quint32 m_uptime = 0;
    quint16 m_heartbeatCounter = 0;
    qint64 m_lastHeartbeatAt = 0;
    bool m_restartCandidate = false;
    quint32 m_candidateUptime = 0;
    quint16 m_candidateCounter = 0;
    qint64 m_candidateAt = 0;
    bool m_pending = false;
    quint16 m_nextSequence = 1;
    quint16 m_pendingSequence = 0;
    quint8 m_pendingOperation = 0;
    bool m_desiredLed = false;
    qint64 m_commandAt = 0;
    QMap<QString, quint64> m_counters;
    QJsonArray m_results;
};

#endif
