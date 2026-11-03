// SPDX-License-Identifier: MIT
#include "aapprotocol.h"

#include <QTest>

using Component = BatteryPart::Component;

class AapProtocolTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Captured from real AirPods: left 97 %, right 100 %, both discharging, case out of reach.
    void parsesRealBatteryPacket()
    {
        const auto packet = QByteArray::fromHex("04000400040003" "0401610201" "0201640201" "0801000401");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->size(), 2); // the unreachable case is left out
        QCOMPARE(parts->at(0), (BatteryPart{Component::Left, 97, false}));
        QCOMPARE(parts->at(1), (BatteryPart{Component::Right, 100, false}));
    }

    void reportsChargingCaseAndSingleBattery()
    {
        const auto packet = QByteArray::fromHex("04000400040002" "0801370101" "0101200201");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->size(), 2);
        QCOMPARE(parts->at(0), (BatteryPart{Component::Case, 55, true}));
        QCOMPARE(parts->at(1), (BatteryPart{Component::Main, 32, false}));
    }

    void ignoresOtherPackets_data()
    {
        QTest::addColumn<QByteArray>("packet");
        QTest::newRow("empty") << QByteArray();
        QTest::newRow("handshake reply") << QByteArray::fromHex("010004000000010003000000000000000000");
        QTest::newRow("ear detection") << QByteArray::fromHex("0400040006000000");
        QTest::newRow("truncated battery") << QByteArray::fromHex("04000400040002040161");
    }

    void ignoresOtherPackets()
    {
        QFETCH(QByteArray, packet);
        QVERIFY(!Aap::parseBatteryPacket(packet).has_value());
    }

    void outgoingPacketsMatchProtocol()
    {
        QCOMPARE(Aap::handshakePacket().toHex(), QByteArray("00000400010002000000000000000000"));
        QCOMPARE(Aap::requestNotificationsPacket().toHex(), QByteArray("040004000f00ffffffff"));
    }
};

QTEST_GUILESS_MAIN(AapProtocolTest)
#include "aapprotocoltest.moc"
