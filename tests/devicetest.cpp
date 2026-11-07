// SPDX-License-Identifier: MIT
#include "device.h"

#include <QTest>

using Component = BatteryPart::Component;

namespace
{
Device makeDevice(const QString &id, const QString &name, int percentage = -1)
{
    Device device;
    device.id = id;
    device.name = name;
    device.percentage = percentage;
    return device;
}

Device makeGone(const QString &id, const QString &name, qint64 lastSeen, int percentage = -1)
{
    Device device = makeDevice(id, name, percentage);
    device.connected = false;
    device.lastSeen = lastSeen;
    return device;
}
}

class DeviceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void overallLevelIsEmptiestEarbud()
    {
        Device device;
        applyBatteryParts(device,
                          {
                              {Component::Case, 10, false},
                              {Component::Right, 80, false},
                              {Component::Left, 40, true},
                          });

        QCOMPARE(device.kind, DeviceKind::Earbuds);
        // The left bud charges in the case, so the right one is the one in use; the case is
        // ignored while a bud is in use.
        QCOMPARE(device.percentage, 80);
        QVERIFY(!device.charging);
        QCOMPARE(device.parts.size(), 3);
        QCOMPARE(device.parts.at(0).component, Component::Left); // sorted Left, Right, Case
        QCOMPARE(device.parts.at(2).component, Component::Case);
    }

    // In the ear at 50 %, the other bud charging in the case at 30 %: show the one in the ear.
    void showsTheEarbudInUse()
    {
        Device device;
        applyBatteryParts(device, {{Component::Left, 50, false}, {Component::Right, 30, true}, {Component::Case, 50, false}});

        QCOMPARE(device.percentage, 50);
        QVERIFY(!device.charging);
    }

    // Both buds charging in the open case: the emptiest of them, as charging.
    void bothEarbudsInTheCase()
    {
        Device device;
        applyBatteryParts(device, {{Component::Left, 60, true}, {Component::Right, 45, true}, {Component::Case, 80, false}});

        QCOMPARE(device.percentage, 45);
        QVERIFY(device.charging);
    }

    void fallsBackToCaseWhenBudsAreDocked()
    {
        Device device;
        applyBatteryParts(device, {{Component::Case, 65, true}});

        QCOMPARE(device.percentage, 65);
        QVERIFY(device.charging);
    }

    void singleBatteryHasNoParts()
    {
        Device device;
        applyBatteryParts(device, {{Component::Main, 72, false}});

        QCOMPARE(device.kind, DeviceKind::Headphones);
        QCOMPARE(device.percentage, 72);
        QVERIFY(device.parts.isEmpty());
    }

    // An earbud without a reading (level byte > 100) must not hide the level of the other.
    void unknownEarbudDoesNotHideTheOther()
    {
        Device device;
        applyBatteryParts(device, {{Component::Left, 5, false}, {Component::Right, -1, false}, {Component::Case, 60, false}});

        QCOMPARE(device.percentage, 5);
        QVERIFY(!device.charging);
        QCOMPARE(device.parts.size(), 3); // the popup still lists the silent earbud
    }

    // One AirPod in the ear, the other charging in the case: the level shown is the one in the
    // ear, which is running down, so the device must not count as charging.
    void chargingFlagFollowsTheShownPart()
    {
        Device device;
        applyBatteryParts(device, {{Component::Left, 10, false}, {Component::Right, 30, true}, {Component::Case, 50, false}});

        QCOMPARE(device.percentage, 10);
        QVERIFY(!device.charging);
    }

    void noKnownLevelIsUnknown()
    {
        Device device;
        applyBatteryParts(device, {{Component::Left, -1, false}, {Component::Case, -1, true}});

        QCOMPARE(device.percentage, -1);
        QVERIFY(!device.charging);
    }

    // AirPods report placements for the primary and secondary earbud; the primary is the
    // first earbud of the battery report, whatever the side.
    void placementsFollowThePrimaryEarbud()
    {
        QList<BatteryPart> parts{{Component::Case, 50, false}, {Component::Right, 80, false}, {Component::Left, 40, false}};
        applyPlacements(parts, Placement::InEar, Placement::InCase);

        QCOMPARE(parts.at(0).placement, Placement::Unknown); // the case has none
        QCOMPARE(parts.at(1).placement, Placement::InEar); // right, listed first
        QCOMPARE(parts.at(2).placement, Placement::InCase);
    }

    // With in-ear detection, the earbud in the ear wins even over an emptier one that lies
    // on the table (not charging, so without detection it would count as in use).
    void showsTheEarbudInTheEar()
    {
        Device device;
        applyBatteryParts(device,
                          {{Component::Left, 30, false, Placement::OutOfEar}, {Component::Right, 70, false, Placement::InEar}, {Component::Case, 50, false}});

        QCOMPARE(device.percentage, 70);
        QCOMPARE(device.parts.at(1).placement, Placement::InEar);
    }

    // Both out of the ears: the emptiest earbud, as without detection.
    void noEarbudInTheEar()
    {
        Device device;
        applyBatteryParts(device,
                          {{Component::Left, 30, false, Placement::OutOfEar}, {Component::Right, 70, true, Placement::InCase}, {Component::Case, 50, false}});

        QCOMPARE(device.percentage, 30);
        QVERIFY(!device.charging);
    }

    void modelNames_data()
    {
        QTest::addColumn<QString>("modalias");
        QTest::addColumn<QString>("model");
        QTest::newRow("AirPods Pro 2") << QStringLiteral("bluetooth:v004Cp2014d0B5A") << QStringLiteral("AirPods Pro (2nd generation)");
        QTest::newRow("lower case") << QStringLiteral("bluetooth:v004cp201bd215c") << QStringLiteral("AirPods 4 (ANC)");
        QTest::newRow("unknown product") << QStringLiteral("bluetooth:v004CpFFFFd0001") << QString();
        QTest::newRow("other vendor") << QStringLiteral("bluetooth:v0075p2014d0001") << QString();
        QTest::newRow("usb") << QStringLiteral("usb:v05ACp2014d0001") << QString();
    }

    void modelNames()
    {
        QFETCH(QString, modalias);
        QFETCH(QString, model);
        QCOMPARE(appleModelName(modalias), model);
    }

    void recentDevicesKeepLastLevels()
    {
        Device airPods = makeDevice(QStringLiteral("aap:1"), QStringLiteral("AirPods"));
        airPods.address = QStringLiteral("38:C4:3A:7B:06:85");
        applyBatteryParts(airPods, {{Component::Left, 40, false, Placement::InEar}, {Component::Case, 60, true}});
        airPods.noiseControl = NoiseControl::Transparency;

        const QList<Device> recent = updateRecentDevices({}, {airPods}, {}, 1000);
        QCOMPARE(recent.size(), 1);
        const Device &kept = recent.first();
        QVERIFY(!kept.connected);
        QCOMPARE(kept.lastSeen, 1000);
        QCOMPARE(kept.percentage, 40);
        QCOMPARE(kept.noiseControl, NoiseControl::Unknown);
        QCOMPARE(kept.parts.size(), 2);
        QVERIFY(!kept.parts.at(1).charging); // a last level, not charging any more
        QCOMPARE(kept.parts.at(0).placement, Placement::Unknown);
    }

    void recentDevicesDropReconnectedAndOld()
    {
        QList<Device> recent;
        for (int i = 0; i < MaxRecentDevices; ++i) {
            recent.append(makeGone(QStringLiteral("old%1").arg(i), QStringLiteral("Old %1").arg(i), 100 - i));
        }
        // "Old 0" is back under a new id (e.g. a Logitech receiver), a new device just went away.
        const Device back = makeDevice(QStringLiteral("new-id"), QStringLiteral("Old 0"));
        const Device gone = makeDevice(QStringLiteral("gone"), QStringLiteral("Gone"));
        const QList<Device> updated = updateRecentDevices(recent, {gone}, {back}, 500);

        QCOMPARE(updated.size(), MaxRecentDevices);
        QCOMPARE(updated.first().id, QStringLiteral("gone")); // the most recent first
        QVERIFY(std::ranges::none_of(updated, [](const Device &device) {
            return device.name == QStringLiteral("Old 0");
        }));
        QCOMPARE(updated.last().id, QStringLiteral("old%1").arg(MaxRecentDevices - 1));
    }

    void recentDevicesRoundTripThroughJson()
    {
        Device mouse = makeDevice(QStringLiteral("upower:/mouse"), QStringLiteral("Mouse"), 30);
        mouse.kind = DeviceKind::Mouse;
        mouse.coarseLevel = CoarseLevel::Low;
        Device airPods = makeDevice(QStringLiteral("aap:1"), QStringLiteral("AirPods"));
        airPods.address = QStringLiteral("38:C4:3A:7B:06:85");
        applyBatteryParts(airPods, {{Component::Left, 40, false}, {Component::Right, -1, false}});
        airPods.model = QStringLiteral("AirPods Pro");
        const QList<Device> recent = updateRecentDevices({}, {mouse, airPods}, {}, 1234);

        QCOMPARE(recentDevicesFromJson(recentDevicesToJson(recent)), recent);
    }

    void recentDevicesSkipMalformedJson_data()
    {
        QTest::addColumn<QByteArray>("json");
        QTest::newRow("not json") << QByteArray("garbage");
        QTest::newRow("not an array") << QByteArray(R"({"id": "x"})");
        QTest::newRow("no id") << QByteArray(R"([{"lastSeen": 5}])");
        QTest::newRow("no time") << QByteArray(R"([{"id": "x"}])");
        QTest::newRow("bad kind") << QByteArray(R"([{"id": "x", "lastSeen": 5, "kind": 99}])");
        QTest::newRow("bad level") << QByteArray(R"([{"id": "x", "lastSeen": 5, "percentage": 250}])");
    }

    void recentDevicesSkipMalformedJson()
    {
        QFETCH(QByteArray, json);
        QVERIFY(recentDevicesFromJson(json).isEmpty());
    }

    void disconnectedDeviceIsNeverTheLowest()
    {
        const Device gone = makeGone(QStringLiteral("a"), QStringLiteral("A"), 1, 5);
        const Device mouse = makeDevice(QStringLiteral("b"), QStringLiteral("B"), 50);
        QCOMPARE(lowestDeviceIndex({gone, mouse}), 1);
        QCOMPARE(lowestDeviceIndex({gone}), -1);
    }

    void lowestDevice_data()
    {
        QTest::addColumn<QList<Device>>("devices");
        QTest::addColumn<int>("lowest");
        auto device = [](int percentage, bool charging) {
            Device d;
            d.percentage = percentage;
            d.charging = charging;
            return d;
        };
        QTest::newRow("none") << QList<Device>{} << -1;
        QTest::newRow("only unknown") << QList<Device>{device(-1, false)} << -1;
        QTest::newRow("lowest") << QList<Device>{device(40, false), device(12, false), device(30, false)} << 1;
        QTest::newRow("unknown ignored") << QList<Device>{device(-1, false), device(80, false)} << 1;
        // A keyboard at 12 % running down matters more than a mouse at 5 % on its charger.
        QTest::newRow("charging comes last") << QList<Device>{device(12, false), device(5, true)} << 0;
        QTest::newRow("all charging") << QList<Device>{device(12, true), device(5, true)} << 1;
        QTest::newRow("tie keeps first") << QList<Device>{device(20, false), device(20, false)} << 0;
    }

    void lowestDevice()
    {
        QFETCH(QList<Device>, devices);
        QFETCH(int, lowest);
        QCOMPARE(lowestDeviceIndex(devices), lowest);
    }

    void vanishedDevicesSkipsReplacements()
    {
        auto device = [](const QString &id, const QString &name, DeviceKind kind, const QString &address = {}) {
            Device d;
            d.id = id;
            d.name = name;
            d.kind = kind;
            d.address = address;
            return d;
        };
        const QList<Device> shown{
            device(QStringLiteral("upower:hidpp_battery_3"), QStringLiteral("M720"), DeviceKind::Mouse),
            device(QStringLiteral("aap:38"), QStringLiteral("AirPods"), DeviceKind::Earbuds, QStringLiteral("38")),
            device(QStringLiteral("upower:kbd"), QStringLiteral("K380"), DeviceKind::Keyboard),
            device(QStringLiteral("upower:pad"), QStringLiteral("Pad"), DeviceKind::Gamepad),
        };
        const QList<Device> next{
            device(QStringLiteral("upower:hidpp_battery_4"), QStringLiteral("M720"), DeviceKind::Mouse), // recreated object
            device(QStringLiteral("upower:headset"), QStringLiteral("AirPods (HFP)"), DeviceKind::Headset, QStringLiteral("38")), // fallback
            device(QStringLiteral("upower:kbd"), QStringLiteral("K380"), DeviceKind::Keyboard), // unchanged
        };

        const QList<Device> vanished = vanishedDevices(shown, next);
        QCOMPARE(vanished.size(), 1);
        QCOMPARE(vanished.at(0).id, QStringLiteral("upower:pad"));
    }

    void extractsBluetoothAddress_data()
    {
        QTest::addColumn<QString>("text");
        QTest::addColumn<QString>("address");
        QTest::newRow("bluez path") << QStringLiteral("/org/bluez/hci0/dev_38_C4_3A_7B_06_85") << QStringLiteral("38:C4:3A:7B:06:85");
        QTest::newRow("hid battery") << QStringLiteral("hid-38:c4:3a:7b:06:85-battery") << QStringLiteral("38:C4:3A:7B:06:85");
        QTest::newRow("usb receiver") << QStringLiteral("hidpp_battery_0") << QString();
    }

    void extractsBluetoothAddress()
    {
        QFETCH(QString, text);
        QFETCH(QString, address);
        QCOMPARE(bluetoothAddressIn(text), address);
    }
};

QTEST_GUILESS_MAIN(DeviceTest)
#include "devicetest.moc"
