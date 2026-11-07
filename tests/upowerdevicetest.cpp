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

    // Devices with only a rough level: UPower's Percentage is a made-up value then.
    void coarseLevelReplacesPercentage_data()
    {
        QTest::addColumn<uint>("batteryLevel");
        QTest::addColumn<CoarseLevel>("level");
        QTest::addColumn<int>("percentage");
        QTest::newRow("low") << 3u << CoarseLevel::Low << 15;
        QTest::newRow("critical") << 4u << CoarseLevel::Critical << 5;
        QTest::newRow("normal") << 6u << CoarseLevel::Normal << 50;
        QTest::newRow("high") << 7u << CoarseLevel::High << 80;
        QTest::newRow("full") << 8u << CoarseLevel::Full << 100;
    }

    void coarseLevelReplacesPercentage()
    {
        QFETCH(uint, batteryLevel);
        QFETCH(CoarseLevel, level);
        QFETCH(int, percentage);
        const Device device = UPower::toDevice(
            Path,
            {{QStringLiteral("BatteryLevel"), batteryLevel}, {QStringLiteral("Percentage"), 55.0}, {QStringLiteral("State"), StateDischarging}});
        QCOMPARE(device.coarseLevel, level);
        QCOMPARE(device.percentage, percentage);
    }

    // None (1), Unknown (0) or missing: the percentage is real.
    void noCoarseLevelKeepsPercentage_data()
    {
        QTest::addColumn<QVariant>("batteryLevel");
        QTest::newRow("none") << QVariant(1u);
        QTest::newRow("unknown") << QVariant(0u);
        QTest::newRow("missing") << QVariant();
    }

    void noCoarseLevelKeepsPercentage()
    {
        QFETCH(QVariant, batteryLevel);
        QVariantMap properties{{QStringLiteral("Percentage"), 73.0}, {QStringLiteral("State"), StateDischarging}};
        if (batteryLevel.isValid()) {
            properties.insert(QStringLiteral("BatteryLevel"), batteryLevel);
        }
        const Device device = UPower::toDevice(Path, properties);
        QCOMPARE(device.coarseLevel, CoarseLevel::None);
        QCOMPARE(device.percentage, 73);
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

    // Every UPower kind maps to a kind with an icon that exists (in Breeze); the ones Breeze has
    // no icon for get the generic one.
    void mapsEveryUPowerType_data()
    {
        QTest::addColumn<uint>("type");
        QTest::addColumn<QString>("icon");
        QTest::newRow("monitor") << 4u << QStringLiteral("video-display");
        QTest::newRow("pda") << 7u << QStringLiteral("phone");
        QTest::newRow("computer") << 11u << QStringLiteral("computer");
        QTest::newRow("pen") << 13u << QStringLiteral("input-tablet");
        QTest::newRow("modem") << 15u << QStringLiteral("network-modem");
        QTest::newRow("network") << 16u << QStringLiteral("network-wireless");
        QTest::newRow("video") << 20u << QStringLiteral("camera-video");
        QTest::newRow("other audio") << 21u << QStringLiteral("audio-headphones");
        QTest::newRow("remote control") << 22u << QStringLiteral("preferences-system-bluetooth-battery-symbolic");
        QTest::newRow("printer") << 23u << QStringLiteral("printer");
        QTest::newRow("scanner") << 24u << QStringLiteral("scanner");
        QTest::newRow("camera") << 25u << QStringLiteral("camera-photo");
        QTest::newRow("wearable") << 26u << QStringLiteral("preferences-system-bluetooth-battery-symbolic");
        QTest::newRow("toy") << 27u << QStringLiteral("applications-games");
        QTest::newRow("bluetooth generic") << 28u << QStringLiteral("preferences-system-bluetooth");
        QTest::newRow("unknown") << 0u << QStringLiteral("preferences-system-bluetooth-battery-symbolic");
    }

    void mapsEveryUPowerType()
    {
        QFETCH(uint, type);
        QFETCH(QString, icon);
        QCOMPARE(iconName(UPower::toDevice(Path, {{QStringLiteral("Model"), QStringLiteral("Gadget")}, {QStringLiteral("Type"), type}}).kind), icon);
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
