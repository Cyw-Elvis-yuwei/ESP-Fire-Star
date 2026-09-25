#ifndef CANBENCH_NORMALRUN_H
#define CANBENCH_NORMALRUN_H

#include <QObject>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>

class DiagnosticEngine;

// Bounded normal traffic only. DiagnosticEngine remains the sole protocol matcher.
class NormalRun : public QObject {
    Q_OBJECT
public:
    explicit NormalRun(DiagnosticEngine *engine, QObject *parent = nullptr);
    bool start(const QString &newDirectory, QString *error);
    void cancel();
    bool running() const { return m_running; }
    QString directory() const { return m_directory; }
    QJsonObject report() const { return m_report; }
signals:
    void progress(QString text);
    void finished(bool passed, QString text);
private:
    void advance();
    void commandFinished(quint16 seq, quint8 op, bool success, QString outcome, qint64 latency);
    void finish(const QString &outcome);
    bool save(QString *error);
    DiagnosticEngine *m_engine;
    QTimer m_next;
    bool m_running = false;
    bool m_waiting = false;
    bool m_initialLed = false;
    quint16 m_sequence = 0;
    int m_index = 0;
    QString m_directory;
    QJsonArray m_steps;
    QJsonObject m_report;
};
#endif
