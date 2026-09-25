#include <QtTest>
#include "../core/socketcanidentity.h"

class IdentityTest : public QObject {
    Q_OBJECT
private slots:
    void classify_data();
    void classify();
    void actualInterfaceWhenRequested();
};

void IdentityTest::classify_data()
{
    QTest::addColumn<QByteArray>("json");
    QTest::addColumn<bool>("valid");
    QTest::addColumn<bool>("simulated");
    QTest::newRow("slcan-no-kind") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\"}]") << true << false;
    QTest::newRow("physical-can") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\",\"linkinfo\":{\"info_kind\":\"can\"}}]") << true << false;
    QTest::newRow("explicit-slcan") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\",\"linkinfo\":{\"info_kind\":\"slcan\"}}]") << true << false;
    QTest::newRow("vcan-renamed-can0") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\",\"linkinfo\":{\"info_kind\":\"vcan\"}}]") << true << true;
    QTest::newRow("vxcan") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\",\"linkinfo\":{\"info_kind\":\"vxcan\"}}]") << true << true;
    QTest::newRow("ethernet") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"ether\"}]") << false << true;
    QTest::newRow("wrong-name") << QByteArray("[{\"ifname\":\"can1\",\"link_type\":\"can\"}]") << false << true;
    QTest::newRow("unknown-kind") << QByteArray("[{\"ifname\":\"can0\",\"link_type\":\"can\",\"linkinfo\":{\"info_kind\":\"unreviewed\"}}]") << false << true;
    QTest::newRow("missing-type") << QByteArray("[{\"ifname\":\"can0\"}]") << false << true;
    QTest::newRow("empty") << QByteArray("[]") << false << true;
    QTest::newRow("multiple") << QByteArray("[{},{}]") << false << true;
    QTest::newRow("malformed") << QByteArray("not json") << false << true;
}

void IdentityTest::classify()
{
    QFETCH(QByteArray, json);
    QFETCH(bool, valid);
    QFETCH(bool, simulated);
    bool actual = true;
    QString error;
    QCOMPARE(SocketCanIdentity::fromIpJson(json, QStringLiteral("can0"), &actual, &error), valid);
    QCOMPARE(actual, simulated);
    QCOMPARE(error.isEmpty(), valid);
}

void IdentityTest::actualInterfaceWhenRequested()
{
    const QString name = qEnvironmentVariable("CANBENCH_TEST_INTERFACE");
    if (name.isEmpty()) QSKIP("No live interface requested");
    bool simulated = true;
    QString error;
    QVERIFY2(SocketCanIdentity::inspect(name, &simulated, &error), qPrintable(error));
    QCOMPARE(simulated, qEnvironmentVariableIntValue("CANBENCH_TEST_SIMULATED") != 0);
    QVERIFY(!SocketCanIdentity::inspect(QStringLiteral("can0;bad"), &simulated, &error));
}

QTEST_GUILESS_MAIN(IdentityTest)
#include "test_socketcanidentity.moc"
