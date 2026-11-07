// SPDX-License-Identifier: MIT
#include "aapprotocol.h"

#include <QRandomGenerator>
#include <QTest>

using Component = BatteryPart::Component;

class AapProtocolTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Captured from real AirPods: left 97 %, right 100 %, both discharging, case out of reach.
    void parsesRealBatteryPacket()
    {
        const auto packet = QByteArray::fromHex(
            "04000400040003"
            "0401610201"
            "0201640201"
            "0801000401");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->size(), 2); // the unreachable case is left out
        QCOMPARE(parts->at(0), (BatteryPart{Component::Left, 97, false}));
        QCOMPARE(parts->at(1), (BatteryPart{Component::Right, 100, false}));
    }

    void reportsChargingCaseAndSingleBattery()
    {
        const auto packet = QByteArray::fromHex(
            "04000400040002"
            "0801370101"
            "0101200201");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->size(), 2);
        QCOMPARE(parts->at(0), (BatteryPart{Component::Case, 55, true}));
        QCOMPARE(parts->at(1), (BatteryPart{Component::Main, 32, false}));
    }

    void skipsUnknownComponents()
    {
        // 0x10 is not a component we know; the right bud next to it must still be read.
        const auto packet = QByteArray::fromHex(
            "04000400040002"
            "1001500201"
            "0201640201");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->size(), 1);
        QCOMPARE(parts->at(0), (BatteryPart{Component::Right, 100, false}));
    }

    void outOfRangeLevelIsUnknown()
    {
        const auto packet = QByteArray::fromHex(
            "04000400040001"
            "0401ff0201");
        const auto parts = Aap::parseBatteryPacket(packet);

        QVERIFY(parts.has_value());
        QCOMPARE(parts->at(0), (BatteryPart{Component::Left, -1, false}));
    }

    void emptyBatteryPacketHasNoParts()
    {
        const auto parts = Aap::parseBatteryPacket(QByteArray::fromHex("04000400040000"));

        QVERIFY(parts.has_value());
        QVERIFY(parts->isEmpty());
    }

    // Every prefix of a valid packet is either rejected or fully parsed, never read past its end.
    void rejectsEveryTruncation()
    {
        const auto packet = QByteArray::fromHex(
            "04000400040003"
            "0401610201"
            "0201640201"
            "0801000401");
        for (qsizetype size = 0; size < packet.size(); ++size) {
            QVERIFY2(!Aap::parseBatteryPacket(packet.first(size)).has_value(), qPrintable(QString::number(size)));
        }
    }

    // The parser runs inside plasmashell on bytes from a radio: arbitrary input must be safe.
    // Run under ASan/UBSan to catch out-of-bounds reads, not just crashes.
    void survivesRandomInput()
    {
        QRandomGenerator random(42); // fixed seed: failures must be reproducible
        for (int i = 0; i < 20000; ++i) {
            QByteArray packet(random.bounded(64), Qt::Uninitialized);
            for (char &byte : packet) {
                byte = char(random.bounded(256));
            }
            if (random.bounded(2)) { // half of them look like battery packets, to get past the header check
                packet.prepend(QByteArray::fromHex("040004000400"));
            }
            if (const auto parts = Aap::parseBatteryPacket(packet)) {
                for (const BatteryPart &part : *parts) {
                    QVERIFY(part.percentage >= -1 && part.percentage <= 100);
                }
            }
        }
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

    void parsesEarDetection_data()
    {
        QTest::addColumn<QByteArray>("packet");
        QTest::addColumn<Placement>("primary");
        QTest::addColumn<Placement>("secondary");
        QTest::newRow("both in ears") << QByteArray::fromHex("0400040006000000") << Placement::InEar << Placement::InEar;
        QTest::newRow("one out, one in the case") << QByteArray::fromHex("0400040006000102") << Placement::OutOfEar << Placement::InCase;
        QTest::newRow("unknown state") << QByteArray::fromHex("0400040006000700") << Placement::Unknown << Placement::InEar;
    }

    void parsesEarDetection()
    {
        QFETCH(QByteArray, packet);
        QFETCH(Placement, primary);
        QFETCH(Placement, secondary);
        const auto placements = Aap::parseEarDetectionPacket(packet);
        QVERIFY(placements.has_value());
        QCOMPARE(placements->first, primary);
        QCOMPARE(placements->second, secondary);
        QVERIFY(!Aap::parseBatteryPacket(packet).has_value());
    }

    void parsesNoiseControl_data()
    {
        QTest::addColumn<QByteArray>("packet");
        QTest::addColumn<NoiseControl>("mode");
        QTest::newRow("off") << QByteArray::fromHex("0400040009000d01000000") << NoiseControl::Off;
        QTest::newRow("cancellation") << QByteArray::fromHex("0400040009000d02000000") << NoiseControl::Cancellation;
        QTest::newRow("transparency") << QByteArray::fromHex("0400040009000d03000000") << NoiseControl::Transparency;
        QTest::newRow("adaptive") << QByteArray::fromHex("0400040009000d04000000") << NoiseControl::Adaptive;
        QTest::newRow("unknown mode") << QByteArray::fromHex("0400040009000d09000000") << NoiseControl::Unknown;
    }

    void parsesNoiseControl()
    {
        QFETCH(QByteArray, packet);
        QFETCH(NoiseControl, mode);
        QCOMPARE(Aap::parseNoiseControlPacket(packet), std::optional(mode));
    }

    void ignoresOtherControlPackets_data()
    {
        QTest::addColumn<QByteArray>("packet");
        QTest::newRow("other setting") << QByteArray::fromHex("0400040009001a01000000");
        QTest::newRow("truncated") << QByteArray::fromHex("0400040009000d");
        QTest::newRow("battery") << QByteArray::fromHex("040004000400010401610201");
    }

    void ignoresOtherControlPackets()
    {
        QFETCH(QByteArray, packet);
        QVERIFY(!Aap::parseNoiseControlPacket(packet).has_value());
        QVERIFY(!Aap::parseEarDetectionPacket(packet).has_value());
    }

    void outgoingPacketsMatchProtocol()
    {
        QCOMPARE(Aap::handshakePacket().toHex(), QByteArray("00000400010002000000000000000000"));
        QCOMPARE(Aap::requestNotificationsPacket().toHex(), QByteArray("040004000f00ffffffff"));
    }
};

QTEST_GUILESS_MAIN(AapProtocolTest)
#include "aapprotocoltest.moc"
