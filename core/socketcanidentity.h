#ifndef CANBENCH_SOCKETCAN_IDENTITY_H
#define CANBENCH_SOCKETCAN_IDENTITY_H
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QRegularExpression>
#include <QString>

namespace SocketCanIdentity {
// Qt 5.12's isVirtual() tests the sysfs path. Real SLCAN is also under
// /sys/devices/virtual/net, so it must not be treated as a simulated node.
inline bool fromIpJson(const QByteArray &data, const QString &name,
                       bool *simulated, QString *error)
{
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isArray()
            || doc.array().size() != 1) {
        if (error) *error = QStringLiteral("无法解析唯一的接口信息");
        return false;
    }
    const QJsonObject link = doc.array().first().toObject();
    if (link.value(QStringLiteral("ifname")).toString() != name
            || link.value(QStringLiteral("link_type")).toString() != QStringLiteral("can")) {
        if (error) *error = QStringLiteral("返回的接口名称或CAN类型不匹配");
        return false;
    }
    const QString kind = link.value(QStringLiteral("linkinfo")).toObject()
            .value(QStringLiteral("info_kind")).toString();
    const bool virtualCan = kind == QStringLiteral("vcan") || kind == QStringLiteral("vxcan");
    // Older Linux slcan exposes link_type=can without IFLA_INFO_KIND.
    if (!virtualCan && !kind.isEmpty() && kind != QStringLiteral("can")
            && kind != QStringLiteral("slcan")) {
        if (error) *error = QStringLiteral("未支持的CAN接口类型：") + kind;
        return false;
    }
    *simulated = virtualCan;
    if (error) error->clear();
    return true;
}

inline bool inspect(const QString &name, bool *simulated, QString *error)
{
    const QRegularExpression valid(QStringLiteral("\\A[A-Za-z0-9_][A-Za-z0-9_.:-]{0,14}\\z"));
    if (!valid.match(name).hasMatch()) {
        if (error) *error = QStringLiteral("接口名称无效");
        return false;
    }
    QProcess process;
    process.start(QStringLiteral("ip"), {QStringLiteral("-j"), QStringLiteral("-d"),
        QStringLiteral("link"), QStringLiteral("show"), QStringLiteral("dev"), name});
    if (!process.waitForStarted(1000) || !process.waitForFinished(2000)) {
        process.kill();
        process.waitForFinished(1000);
        if (error) *error = QStringLiteral("读取接口类型失败或超时");
        return false;
    }
    if (process.exitStatus() != QProcess::NormalExit || process.exitCode() != 0) {
        if (error) *error = QString::fromLocal8Bit(process.readAllStandardError());
        return false;
    }
    return fromIpJson(process.readAllStandardOutput(), name, simulated, error);
}
}
#endif
