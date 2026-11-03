// SPDX-License-Identifier: MIT
#include "upowerdevice.h"

#include <QTest>

namespace
{
const QString Path = QStringLiteral("/org/freedesktop/UPower/devices/mouse_dev_38_C4_3A_7B_06_85");

// UPower "Type" and "State" values used below.
constexpr uint TypeMouse = 5;
constexpr uint TypeKeyboard = 6;
constexpr uint TypeHeadset = 17;
constexpr uint StateUnknown = 0;
constexpr uint StateCharging = 1;
constexpr uint StateDischarging = 2;
}

class UPowerDeviceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    // Properties as UPower reports them for a Bluetooth mouse.
    void mapsBluetoothMouse()
    {
        const Device device = UPower::toDevice(Path,
                                               {
                                                   {QStringLiteral("NativePath"), QStringLiteral("/org/bluez/hci0/dev_38_C4_3A_7B_06_85")},
                                                   {QStringLiteral("Model"), QStringLiteral("MX Master 3")},
                                                   {QStringLiteral("Vendor"), QStringLiteral("Logitech")},
                                                   {QStringLiteral("Type"), TypeMouse},
                                                   {QStringLiteral("State"), StateDischarging},
                                                   {QStringLiteral("Percentage"), 54.6},
                                               });

        QCOMPARE(device.id, QStringLiteral("upower:") + Path);
        QCOMPARE(device.address, QStringLiteral("38:C4:3A:7B:06:85"));
        QCOMPARE(device.name, QStringLiteral("MX Master 3"));
        QCOMPARE(device.kind, DeviceKind::Mouse);
        QCOMPARE(device.percentage, 55);
        QVERIFY(!device.charging);
        QVERIFY(device.parts.isEmpty());
    }

    void reportsCharging()
    {
        const Device device = UPower::toDevice(Path, {{QStringLiteral("State"), StateCharging}, {QStringLiteral("Percentage"), 80.0}});
        QVERIFY(device.charging);
        QCOMPARE(device.percentage, 80);
    }

    // UPower announces a device with 0 % and an unknown state before reading its battery.
    void placeholderLevelIsUnknown()
    {
        const Device device = UPower::toDevice(Path, {{QStringLiteral("State"), StateUnknown}, {QStringLiteral("Percentage"), 0.0}});
        QCOMPARE(device.percentage, -1);
    }

    void emptyBatteryIsNotUnknown()
    {
        const Device device = UPower::toDevice(Path, {{QStringLiteral("State"), StateDischarging}, {QStringLiteral("Percentage"), 0.0}});
        QCOMPARE(device.percentage, 0);
    }

    void kindPrefersModelName_data()
    {
        QTest::addColumn<QString>("model");
        QTest::addColumn<uint>("type");
        QTest::addColumn<DeviceKind>("kind");
        // Logitech Lightspeed receivers report their mouse as a keyboard.
        QTest::newRow("lightspeed mouse") << QStringLiteral("G502 LIGHTSPEED Wireless Gaming Mouse") << TypeKeyboard << DeviceKind::Mouse;
        QTest::newRow("keyboard") << QStringLiteral("K380 Multi-Device Keyboard") << TypeKeyboard << DeviceKind::Keyboard;
        QTest::newRow("type only") << QStringLiteral("WH-1000XM4") << TypeHeadset << DeviceKind::Headset;
        QTest::newRow("unknown type") << QStringLiteral("Gadget") << 999u << DeviceKind::Other;
    }

    void kindPrefersModelName()
    {
        QFETCH(QString, model);
        QFETCH(uint, type);
        QFETCH(DeviceKind, kind);
        QCOMPARE(UPower::toDevice(Path, {{QStringLiteral("Model"), model}, {QStringLiteral("Type"), type}}).kind, kind);
    }

    void nameFallsBackToVendor()
    {
        QCOMPARE(UPower::toDevice(Path, {{QStringLiteral("Vendor"), QStringLiteral("Logitech")}}).name, QStringLiteral("Logitech"));
    }

    void usbReceiverHasNoAddress()
    {
        QVERIFY(UPower::toDevice(Path, {{QStringLiteral("NativePath"), QStringLiteral("hidpp_battery_0")}}).address.isEmpty());
    }

    void filtersPowerSupplies()
    {
        QVERIFY(!UPower::isPeripheral({{QStringLiteral("PowerSupply"), true}})); // laptop battery
        QVERIFY(UPower::isPeripheral({{QStringLiteral("PowerSupply"), false}}));
        QVERIFY(UPower::isPeripheral({}));
    }

    void presenceDefaultsToTrue()
    {
        QVERIFY(UPower::isPresent({}));
        QVERIFY(!UPower::isPresent({{QStringLiteral("IsPresent"), false}}));
    }
};

QTEST_GUILESS_MAIN(UPowerDeviceTest)
#include "upowerdevicetest.moc"
