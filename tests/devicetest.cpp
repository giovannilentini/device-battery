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
        QCOMPARE(device.percentage, 40); // the case is ignored while a bud is in use
        QVERIFY(device.charging);
        QCOMPARE(device.parts.size(), 3);
        QCOMPARE(device.parts.at(0).component, Component::Left); // sorted Left, Right, Case
        QCOMPARE(device.parts.at(2).component, Component::Case);
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
