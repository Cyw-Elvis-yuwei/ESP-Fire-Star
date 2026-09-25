/****************************************************************************
**
** Copyright (C) 2017 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the examples of the QtSerialBus module.
**
** $QT_BEGIN_LICENSE:BSD$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** BSD License Usage
** Alternatively, you may use this file under the terms of the BSD license
** as follows:
**
** "Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions are
** met:
**   * Redistributions of source code must retain the above copyright
**     notice, this list of conditions and the following disclaimer.
**   * Redistributions in binary form must reproduce the above copyright
**     notice, this list of conditions and the following disclaimer in
**     the documentation and/or other materials provided with the
**     distribution.
**   * Neither the name of The Qt Company Ltd nor the names of its
**     contributors may be used to endorse or promote products derived
**     from this software without specific prior written permission.
**
**
** THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
** "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
** LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
** A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
** OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
** SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
** LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
** OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE."
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "connectdialog.h"
#include "diagnosticengine.h"
#include "socketcanidentity.h"
#include "normalrun.h"
#include "continuousrun.h"
#include "recoveryrun.h"
#include <QUuid>

#include <QCanBus>
#include <QCanBusFrame>
#include <QCanBusDeviceInfo>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QTimer>
#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QJsonDocument>
#include <QLabel>
#include <QProcess>
#include <QPushButton>
#include <QPixmap>
#include <QPointer>
#include <QTextDocument>
#include <QVBoxLayout>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::MainWindow)
{
    m_ui->setupUi(this);
    setWindowTitle(QStringLiteral("CAN 设备诊断与自动回归测试台"));
    resize(1000, 760);
    m_ui->receivedMessagesEdit->document()->setMaximumBlockCount(1000);
    m_ui->receivedMessagesBox->setTitle(QStringLiteral("报文与诊断事件（窗口保留最近 1000 行）"));
    m_ui->sendFrameBox->hide();

    m_connectDialog = new ConnectDialog;

    m_status = new QLabel;
    m_ui->statusBar->addPermanentWidget(m_status);

    m_written = new QLabel;
    m_ui->statusBar->addWidget(m_written);

    initActionsConnections();
    initDiagnostics();
}

MainWindow::~MainWindow()
{
    disconnectDevice();

    delete m_connectDialog;
    delete m_ui;
}

void MainWindow::initActionsConnections()
{
    m_ui->actionDisconnect->setEnabled(false);
    m_ui->sendFrameBox->setEnabled(false);

    connect(m_ui->sendFrameBox, &SendFrameBox::sendFrame, this, &MainWindow::sendFrame);
    connect(m_ui->actionConnect, &QAction::triggered, m_connectDialog, &ConnectDialog::show);
    connect(m_connectDialog, &QDialog::accepted, this, &MainWindow::connectDevice);
    connect(m_ui->actionDisconnect, &QAction::triggered, this, &MainWindow::disconnectDevice);
    connect(m_ui->actionQuit, &QAction::triggered, this, &QWidget::close);
    connect(m_ui->actionAboutQt, &QAction::triggered, qApp, &QApplication::aboutQt);
    connect(m_ui->actionClearLog, &QAction::triggered, m_ui->receivedMessagesEdit, &QTextEdit::clear);
    connect(m_ui->actionPluginDocumentation, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(QUrl("http://doc.qt.io/qt-5/qtcanbus-backends.html#can-bus-plugins"));
    });
}

void MainWindow::processErrors(QCanBusDevice::CanBusError error) const
{
    if (!m_canDevice) return;
    switch (error) {
    case QCanBusDevice::ReadError:
    case QCanBusDevice::WriteError:
    case QCanBusDevice::ConnectionError:
    case QCanBusDevice::ConfigurationError:
    case QCanBusDevice::UnknownError:
        m_status->setText(m_canDevice->errorString());
        if (m_engine) m_engine->recordTransportError(m_canDevice->errorString());
        break;
    default:
        break;
    }
}

void MainWindow::connectDevice()
{
    const ConnectDialog::Settings p = m_connectDialog->settings();
    if (p.useConfigurationEnabled) {
        m_result->setText(QStringLiteral("请关闭 Custom configuration；波特率由系统接口配置统一管理。"));
        return;
    }
    openDevice(p.pluginName, p.deviceInterfaceName);
}

void MainWindow::disconnectDevice()
{
    if (!m_canDevice)
        return;

    if (m_engine) {
        m_engine->setConnected(false);
        QString error;
        if (!m_sessionDir.isEmpty() && !m_engine->saveReport(m_sessionDir, &error))
            appendEvent(QStringLiteral("报告保存失败：") + error);
    }
    QCanBusDevice *oldDevice = m_canDevice;
    m_canDevice = nullptr;
    disconnect(oldDevice, nullptr, this, nullptr);
    oldDevice->disconnectDevice();
    oldDevice->deleteLater();

    m_ui->actionConnect->setEnabled(true);
    m_ui->actionDisconnect->setEnabled(false);

    m_ui->sendFrameBox->setEnabled(false);

    m_status->setText(tr("Disconnected"));
    updateDiagnostics();
}

void MainWindow::processFramesWritten(qint64 count)
{
    m_numberFramesWritten += count;
    m_written->setText(tr("%1 frames written").arg(m_numberFramesWritten));
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_connectDialog->close();
    if (m_recoveryRun && m_recoveryRun->running()) {
        m_recoveryResult->setText(QStringLiteral("恢复检查正在执行，请等待完成或点击停止后关闭。"));
        event->ignore();
        return;
    }
    if (m_continuousRun && m_continuousRun->running()) {
        m_continuousResult->setText(QStringLiteral("持续测试正在执行，请等待完成或点击停止后关闭。"));
        event->ignore();
        return;
    }
    if (m_normalRun && m_normalRun->running()) {
        m_normalResult->setText(QStringLiteral("正常回归正在执行，请等待完成或点击停止后关闭。"));
        event->ignore();
        return;
    }
    if (m_testProcess && m_testProcess->state() != QProcess::NotRunning) {
        m_result->setText(QStringLiteral("离线回归仍在运行，请等待完成后关闭窗口。"));
        event->ignore();
        return;
    }
    disconnectDevice();
    event->accept();
}

static QString frameFlags(const QCanBusFrame &frame)
{
    QString result = QLatin1String(" --- ");

    if (frame.hasBitrateSwitch())
        result[1] = QLatin1Char('B');
    if (frame.hasErrorStateIndicator())
        result[2] = QLatin1Char('E');
    if (frame.hasLocalEcho())
        result[3] = QLatin1Char('L');

    return result;
}

void MainWindow::processReceivedFrames()
{
    if (!m_canDevice)
        return;

    while (m_canDevice->framesAvailable()) {
        const QCanBusFrame frame = m_canDevice->readFrame();
        if (m_engine)
            m_engine->receiveFrame(frame);

        QString view;
        if (frame.frameType() == QCanBusFrame::ErrorFrame)
            view = m_canDevice->interpretErrorFrame(frame);
        else
            view = frame.toString();

        const QString time = QString::fromLatin1("%1.%2  ")
                .arg(frame.timeStamp().seconds(), 10, 10, QLatin1Char(' '))
                .arg(frame.timeStamp().microSeconds() / 100, 4, 10, QLatin1Char('0'));

        const QString flags = frameFlags(frame);

        m_ui->receivedMessagesEdit->append((time + flags + view).toHtmlEscaped());
    }
}

void MainWindow::sendFrame(const QCanBusFrame &frame)
{
    if (!m_canDevice)
        return;

    m_canDevice->writeFrame(frame);
}

void MainWindow::initDiagnostics()
{
    auto *panel = new QGroupBox(QStringLiteral("设备诊断"), this);
    auto *layout = new QVBoxLayout(panel);
    m_health = new QLabel(QStringLiteral("尚未连接"), panel);
    m_health->setObjectName(QStringLiteral("deviceHealth"));
    m_health->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600; padding: 8px;"));
    m_telemetry = new QLabel(QStringLiteral("LED：—    运行时间：—    心跳：—"), panel);
    m_result = new QLabel(QStringLiteral("连接 socketcan：can0 为当前实物接口，vcan0 用于模拟验证。"), panel);
    m_result->setObjectName(QStringLiteral("diagnosticResult"));
    m_result->setWordWrap(true);
    m_logPath = new QLabel(QStringLiteral("每次连接自动创建独立日志会话。"), panel);
    m_logPath->setWordWrap(true);
    m_logPath->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(m_health);
    layout->addWidget(m_telemetry);
    auto *buttons = new QHBoxLayout;
    m_ledOn = new QPushButton(QStringLiteral("打开 LED"), panel);
    m_ledOff = new QPushButton(QStringLiteral("关闭 LED"), panel);
    m_query = new QPushButton(QStringLiteral("查询状态"), panel);
    auto *save = new QPushButton(QStringLiteral("保存会话报告"), panel);
    m_regression = new QPushButton(QStringLiteral("运行离线回归"), panel);
    m_ledOn->setObjectName(QStringLiteral("ledOn"));
    m_ledOff->setObjectName(QStringLiteral("ledOff"));
    m_query->setObjectName(QStringLiteral("queryStatus"));
    save->setObjectName(QStringLiteral("saveReport"));
    m_regression->setObjectName(QStringLiteral("runRegression"));
    for (auto *button : {m_ledOn, m_ledOff, m_query, save, m_regression})
        buttons->addWidget(button);
    layout->addLayout(buttons);
    auto *normalRow = new QHBoxLayout;
    m_normalButton = new QPushButton(QStringLiteral("一键真机正常回归"), panel);
    m_normalButton->setObjectName(QStringLiteral("runNormal"));
    m_normalResult = new QLabel(QStringLiteral("7步自动核对：读取 → 关灯并查询 → 开灯并查询 → 恢复并查询。"), panel);
    m_normalResult->setObjectName(QStringLiteral("normalResult"));
    m_normalResult->setWordWrap(true);
    m_normalResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    normalRow->addWidget(m_normalButton);
    normalRow->addWidget(m_normalResult, 1);
    layout->addLayout(normalRow);
    auto *continuousRow = new QHBoxLayout;
    m_continuousButton = new QPushButton(QStringLiteral("持续通信测试（60秒）"), panel);
    m_continuousButton->setObjectName(QStringLiteral("runContinuous"));
    m_continuousResult = new QLabel(QStringLiteral("最多每秒10次状态查询，统计成功率、超时和响应时间；保持LED状态。"), panel);
    m_continuousResult->setObjectName(QStringLiteral("continuousResult"));
    m_continuousResult->setWordWrap(true);
    m_continuousResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    continuousRow->addWidget(m_continuousButton);
    continuousRow->addWidget(m_continuousResult, 1);
    layout->addLayout(continuousRow);
    auto *recoveryRow = new QHBoxLayout;
    m_recoveryButton = new QPushButton(QStringLiteral("节点故障恢复检查"), panel);
    m_recoveryButton->setObjectName(QStringLiteral("runRecovery"));
    m_recoveryResult = new QLabel(QStringLiteral("验证节点无响应、查询超时和恢复后新查询；CANable保持连接。"), panel);
    m_recoveryResult->setObjectName(QStringLiteral("recoveryResult"));
    m_recoveryResult->setWordWrap(true);
    m_recoveryResult->setTextInteractionFlags(Qt::TextSelectableByMouse);
    recoveryRow->addWidget(m_recoveryButton);
    recoveryRow->addWidget(m_recoveryResult, 1);
    layout->addLayout(recoveryRow);
    layout->addWidget(m_result);
    layout->addWidget(m_logPath);
    m_ui->verticalLayout->insertWidget(0, panel);
    connect(m_ledOn, &QPushButton::clicked, this, [this] { if (m_engine) m_engine->sendSetLed(true); });
    connect(m_ledOff, &QPushButton::clicked, this, [this] { if (m_engine) m_engine->sendSetLed(false); });
    connect(m_query, &QPushButton::clicked, this, [this] { if (m_engine) m_engine->sendGetStatus(); });
    connect(save, &QPushButton::clicked, this, &MainWindow::saveReport);
    connect(m_regression, &QPushButton::clicked, this, &MainWindow::runRegression);
    connect(m_normalButton, &QPushButton::clicked, this, &MainWindow::runNormal);
    connect(m_continuousButton, &QPushButton::clicked, this, &MainWindow::runContinuous);
    connect(m_recoveryButton, &QPushButton::clicked, this, &MainWindow::runRecovery);
    updateDiagnostics();
}

void MainWindow::createEngine()
{
    delete m_recoveryRun;
    m_recoveryRun = nullptr;
    delete m_continuousRun;
    m_continuousRun = nullptr;
    delete m_normalRun;
    m_normalRun = nullptr;
    delete m_engine;
    m_engine = new DiagnosticEngine(this);
    m_engine->setSender([this](const QCanBusFrame &frame, QString *error) {
        return writeDiagnosticFrame(frame, error);
    });
    connect(m_engine, &DiagnosticEngine::statusChanged, this, [this](bool, bool) { updateDiagnostics(); });
    connect(m_engine, &DiagnosticEngine::pendingChanged, this, [this](bool) { updateDiagnostics(); });
    connect(m_engine, &DiagnosticEngine::telemetryChanged, this, [this](bool, quint32, quint16) { updateDiagnostics(); });
    connect(m_engine, &DiagnosticEngine::commandFinished, this,
            [this](quint16 seq, quint8 op, bool success, const QString &outcome, qint64 latency) {
        m_result->setText(QStringLiteral("命令 #%1 / 操作 %2：%3 · %4 · %5 ms")
                         .arg(seq).arg(op).arg(success ? QStringLiteral("成功") : QStringLiteral("失败"))
                         .arg(outcome).arg(latency));
        updateDiagnostics();
    });
    connect(m_engine, &DiagnosticEngine::event, this, [this](const QString &name, const QString &detail) {
        appendEvent(name + QStringLiteral(" · ") + detail);
    });
    m_normalRun = new NormalRun(m_engine, this);
    connect(m_normalRun, &NormalRun::progress, this, [this](const QString &text) {
        m_normalResult->setText(text);
        updateDiagnostics();
    });
    connect(m_normalRun, &NormalRun::finished, this, [this](bool, const QString &text) {
        m_normalResult->setText(text);
        appendEvent(text);
        updateDiagnostics();
    });
    m_continuousRun = new ContinuousRun(m_engine, this);
    connect(m_continuousRun, &ContinuousRun::progress, this, [this](const QString &text) {
        m_continuousResult->setText(text);
        updateDiagnostics();
    });
    connect(m_continuousRun, &ContinuousRun::finished, this, [this](bool, const QString &text) {
        m_continuousResult->setText(text);
        appendEvent(text);
        updateDiagnostics();
    });
    m_recoveryRun = new RecoveryRun(m_engine, this);
    connect(m_recoveryRun, &RecoveryRun::progress, this, [this](const QString &text) {
        m_recoveryResult->setText(text);
        updateDiagnostics();
    });
    connect(m_recoveryRun, &RecoveryRun::finished, this, [this](bool, const QString &text) {
        m_recoveryResult->setText(text);
        appendEvent(text);
        updateDiagnostics();
    });
}

bool MainWindow::openDevice(const QString &plugin, const QString &interfaceName)
{
    if (m_recoveryRun && m_recoveryRun->running()) return false;
    if (m_continuousRun && m_continuousRun->running()) return false;
    if (m_normalRun && m_normalRun->running()) return false;
    if (plugin != QStringLiteral("socketcan")) {
        m_result->setText(QStringLiteral("本阶段仅接入 SocketCAN，请选择 socketcan 插件。"));
        return false;
    }
    if (m_testProcess && m_testProcess->state() != QProcess::NotRunning)
        return false;
    disconnectDevice();
    delete m_recoveryRun;
    m_recoveryRun = nullptr;
    delete m_continuousRun;
    m_continuousRun = nullptr;
    delete m_normalRun;
    m_normalRun = nullptr;
    delete m_engine;
    m_engine = nullptr;
    m_sessionDir.clear();
    QString error;
    m_canDevice = QCanBus::instance()->createDevice(plugin, interfaceName, &error);
    if (!m_canDevice) {
        m_result->setText(error);
        return false;
    }
    m_canDevice->setParent(this);
    connect(m_canDevice, &QCanBusDevice::errorOccurred, this, &MainWindow::processErrors);
    connect(m_canDevice, &QCanBusDevice::framesReceived, this, &MainWindow::processReceivedFrames);
    connect(m_canDevice, &QCanBusDevice::framesWritten, this, &MainWindow::processFramesWritten);
    const QPointer<QCanBusDevice> thisDevice(m_canDevice);
    connect(m_canDevice, &QCanBusDevice::stateChanged, this, [this, thisDevice](QCanBusDevice::CanBusDeviceState state) {
        if (state == QCanBusDevice::UnconnectedState) {
            QTimer::singleShot(0, this, [this, thisDevice] {
                if (thisDevice && m_canDevice == thisDevice.data() && m_canDevice->state() == QCanBusDevice::UnconnectedState)
                    disconnectDevice();
            });
        }
    });
    if (!m_canDevice->connectDevice()) {
        m_result->setText(m_canDevice->errorString());
        delete m_canDevice;
        m_canDevice = nullptr;
        return false;
    }
    m_interfaceName = interfaceName;
    m_numberFramesWritten = 0;
    m_written->setText(QStringLiteral("0 frames written"));
    createEngine();
    bool simulated = false;
    QString enumerationError;
    if (!SocketCanIdentity::inspect(interfaceName, &simulated, &enumerationError)) {
        m_result->setText(QStringLiteral("无法确认接口类型，连接取消：") + enumerationError);
        disconnectDevice();
        return false;
    }
    m_engine->setSimulated(simulated);
    m_engine->setInterfaceName(interfaceName);
    m_sessionDir = QDir::cleanPath(QCoreApplication::applicationDirPath() + QStringLiteral("/../artifacts/gui-")
                                  + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
    if (!QDir().mkpath(m_sessionDir) || !m_engine->startLog(m_sessionDir + QStringLiteral("/frames.jsonl"), &error)) {
        m_result->setText(QStringLiteral("无法开启日志，连接取消：") + error);
        disconnectDevice();
        return false;
    }
    m_logPath->setText(QStringLiteral("%1 · 日志：%2")
                      .arg(simulated ? QStringLiteral("模拟会话，不能作为真机验收") : QStringLiteral("真实接口，需另行物理验收"))
                      .arg(m_sessionDir));
    m_engine->setConnected(true);
    m_ui->actionConnect->setEnabled(false);
    m_ui->actionDisconnect->setEnabled(true);
    m_status->setText(QStringLiteral("socketcan → ") + interfaceName);
    m_result->setText(QStringLiteral("接口已连接，可发送诊断命令；设备在线状态由有效心跳确认。"));
    updateDiagnostics();
    return true;
}

bool MainWindow::connectToInterface(const QString &interfaceName)
{
    return openDevice(QStringLiteral("socketcan"), interfaceName);
}

bool MainWindow::writeDiagnosticFrame(const QCanBusFrame &frame, QString *error)
{
    if (!m_canDevice || m_canDevice->state() != QCanBusDevice::ConnectedState) {
        if (error) *error = QStringLiteral("CAN interface is not connected");
        return false;
    }
    const bool accepted = m_canDevice->writeFrame(frame);
    if (accepted)
        appendEvent(QStringLiteral("TX accepted · ") + frame.toString());
    else if (error)
        *error = m_canDevice->errorString();
    return accepted;
}

void MainWindow::updateDiagnostics()
{
    if (!m_health) return;
    const bool connected = m_engine && m_engine->connected();
    const bool online = connected && m_engine->online();
    const bool pending = m_engine && m_engine->pending();
    m_health->setText(!connected ? QStringLiteral("尚未连接")
                               : online ? QStringLiteral("设备在线") : QStringLiteral("等待心跳 / 设备离线"));
    m_health->setStyleSheet(QStringLiteral("font-size: 20px; font-weight: 600; padding: 8px; color: %1;")
                           .arg(online ? QStringLiteral("#087b52") : QStringLiteral("#8b5520")));
    if (m_engine && online)
        m_telemetry->setText(QStringLiteral("LED：%1    运行时间：%2 s    心跳计数：%3    %4")
            .arg(m_engine->led() ? QStringLiteral("开") : QStringLiteral("关"))
            .arg(m_engine->uptimeMs() / 1000.0, 0, 'f', 1).arg(m_engine->heartbeatCounter())
            .arg(pending ? QStringLiteral("等待应答") : QStringLiteral("可发送命令")));
    else m_telemetry->setText(connected ? QStringLiteral("等待有效心跳；当前设备状态尚未确认。")
                                        : QStringLiteral("LED：—    运行时间：—    心跳：—"));
    const bool normalRunning = m_normalRun && m_normalRun->running();
    const bool continuousRunning = m_continuousRun && m_continuousRun->running();
    const bool recoveryRunning = m_recoveryRun && m_recoveryRun->running();
    const bool idlePhysical = online && !pending && !m_engine->report().value("simulated").toBool();
    for (auto *button : {m_ledOn, m_ledOff, m_query}) button->setEnabled(connected && !pending && !normalRunning && !continuousRunning && !recoveryRunning);
    m_normalButton->setText(normalRunning ? QStringLiteral("停止正常回归") : QStringLiteral("一键真机正常回归"));
    m_normalButton->setEnabled(normalRunning || (idlePhysical && !continuousRunning && !recoveryRunning));
    m_continuousButton->setText(continuousRunning ? QStringLiteral("停止持续测试") : QStringLiteral("持续通信测试（60秒）"));
    m_continuousButton->setEnabled(continuousRunning || (idlePhysical && !normalRunning && !recoveryRunning));
    m_recoveryButton->setText(recoveryRunning ? QStringLiteral("停止恢复检查") : QStringLiteral("节点故障恢复检查"));
    m_recoveryButton->setEnabled(recoveryRunning || (idlePhysical && !normalRunning && !continuousRunning));
    m_ui->actionDisconnect->setEnabled(connected && !normalRunning && !continuousRunning && !recoveryRunning);
    m_regression->setEnabled(!connected && (!m_testProcess || m_testProcess->state() == QProcess::NotRunning));
}

void MainWindow::appendEvent(const QString &text)
{
    m_ui->receivedMessagesEdit->append((QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs)
                                      + QStringLiteral("  ") + text).toHtmlEscaped());
}

void MainWindow::saveReport()
{
    if (!m_engine || m_sessionDir.isEmpty()) {
        m_result->setText(QStringLiteral("请先连接设备并产生一个会话。"));
        return;
    }
    QString error;
    m_result->setText(m_engine->saveReport(m_sessionDir, &error)
                     ? QStringLiteral("会话报告已保存：") + m_sessionDir : QStringLiteral("报告保存失败：") + error);
}

void MainWindow::runRegression()
{
    const QDir buildDir(QCoreApplication::applicationDirPath());
    const QString script = QDir::cleanPath(buildDir.absoluteFilePath(QStringLiteral("../tests/run_acceptance.py")));
    const QString cli = QDir::cleanPath(buildDir.absoluteFilePath(QStringLiteral("../build-cli/canbench-cli")));
    const QString output = QDir::cleanPath(buildDir.absoluteFilePath(QStringLiteral("../artifacts/regression-"))
                       + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz")));
    if (!QFileInfo::exists(script) || !QFileInfo::exists(cli)) {
        m_result->setText(QStringLiteral("请先运行 tools/build.sh，生成命令行验收程序。"));
        return;
    }
    if (m_canDevice || (m_testProcess && m_testProcess->state() != QProcess::NotRunning)) return;
    if (m_testProcess) m_testProcess->deleteLater();
    m_testProcess = new QProcess(this);
    m_testProcess->setProgram(QStringLiteral("python3"));
    m_testProcess->setArguments({script, QStringLiteral("--cli"), cli, QStringLiteral("--interface"),
                                QStringLiteral("vcan0"), QStringLiteral("--output"), output});
    connect(m_testProcess, &QProcess::readyReadStandardOutput, this, [this] {
        appendEvent(QString::fromUtf8(m_testProcess->readAllStandardOutput()).trimmed());
    });
    connect(m_testProcess, &QProcess::readyReadStandardError, this, [this] {
        appendEvent(QString::fromUtf8(m_testProcess->readAllStandardError()).trimmed());
    });
    connect(m_testProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        m_result->setText(QStringLiteral("回归程序未能运行：") + m_testProcess->errorString());
        updateDiagnostics();
    });
    connect(m_testProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this, output](int code, QProcess::ExitStatus status) {
        m_result->setText((code == 0 && status == QProcess::NormalExit ? QStringLiteral("模拟回归通过：")
                                                                     : QStringLiteral("回归失败，请查看报告：")) + output);
        updateDiagnostics();
    });
    m_result->setText(QStringLiteral("正在 vcan0 上运行离线回归；不访问真实 CAN 设备。"));
    m_testProcess->start();
    updateDiagnostics();
}

void MainWindow::runNormal()
{
    if (m_recoveryRun && m_recoveryRun->running()) return;
    if (m_continuousRun && m_continuousRun->running()) return;
    if (!m_normalRun || !m_engine) return;
    if (m_normalRun->running()) { m_normalRun->cancel(); return; }
    bool simulated = true;
    QString error;
    if (!SocketCanIdentity::inspect(m_interfaceName, &simulated, &error) || simulated) {
        m_normalResult->setText(QStringLiteral("此按钮需要真实CAN接口；虚拟故障测试请使用离线回归。") + error);
        return;
    }
    const QString output = QDir(m_sessionDir).filePath(QStringLiteral("normal-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
        + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (!m_normalRun->start(output, &error)) m_normalResult->setText(error);
    updateDiagnostics();
}

void MainWindow::runContinuous()
{
    if (m_recoveryRun && m_recoveryRun->running()) return;
    if (!m_continuousRun || !m_engine || (m_normalRun && m_normalRun->running())) return;
    if (m_continuousRun->running()) { m_continuousRun->cancel(); return; }
    bool simulated = true;
    QString error;
    if (!SocketCanIdentity::inspect(m_interfaceName, &simulated, &error) || simulated) {
        m_continuousResult->setText(QStringLiteral("此按钮需要真实CAN接口。") + error);
        return;
    }
    const QString output = QDir(m_sessionDir).filePath(QStringLiteral("continuous-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
        + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (!m_continuousRun->start(output, &error)) m_continuousResult->setText(error);
    updateDiagnostics();
}

void MainWindow::runRecovery()
{
    if (!m_recoveryRun || !m_engine || (m_normalRun && m_normalRun->running())
        || (m_continuousRun && m_continuousRun->running())) return;
    if (m_recoveryRun->running()) { m_recoveryRun->cancel(); return; }
    bool simulated = true;
    QString error;
    if (!SocketCanIdentity::inspect(m_interfaceName, &simulated, &error) || simulated) {
        m_recoveryResult->setText(QStringLiteral("此按钮需要真实CAN接口。") + error);
        return;
    }
    const QString output = QDir(m_sessionDir).filePath(QStringLiteral("recovery-")
        + QDateTime::currentDateTimeUtc().toString(QStringLiteral("yyyyMMdd-HHmmss-zzz"))
        + "-" + QUuid::createUuid().toString(QUuid::WithoutBraces).left(8));
    if (!m_recoveryRun->start(output, &error)) m_recoveryResult->setText(error);
    updateDiagnostics();
}

bool MainWindow::captureTo(const QString &path)
{
    return grab().save(path);
}

bool MainWindow::regressionRunning() const
{
    return (m_normalRun && m_normalRun->running())
        || (m_recoveryRun && m_recoveryRun->running())
        || (m_continuousRun && m_continuousRun->running())
        || (m_testProcess && m_testProcess->state() != QProcess::NotRunning);
}
