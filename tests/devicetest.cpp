// SPDX-License-Identifier: MIT
#include "device.h"

#include <QTest>

using Component = BatteryPart::Component;

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
