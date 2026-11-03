// SPDX-License-Identifier: MIT
#include "upowerdevice.h"

namespace
{
// Values of the UPower "Type" and "State" properties we care about.
enum UPowerType : uint {
    Mouse = 5,
    Keyboard = 6,
    Phone = 8,
    MediaPlayer = 9,
    Tablet = 10,
    GamingInput = 12,
    Pen = 13,
    Touchpad = 14,
    Headset = 17,
    Speakers = 18,
    Headphones = 19,
    OtherAudio = 21,
};
constexpr uint StateUnknown = 0;
constexpr uint StateCharging = 1;

DeviceKind kindFromUPowerType(uint type)
{
    switch (type) {
    case Mouse:
        return DeviceKind::Mouse;
    case Keyboard:
        return DeviceKind::Keyboard;
    case Phone:
        return DeviceKind::Phone;
    case MediaPlayer:
        return DeviceKind::MediaPlayer;
    case Tablet:
    case Pen:
        return DeviceKind::Tablet;
    case GamingInput:
        return DeviceKind::Gamepad;
    case Touchpad:
        return DeviceKind::Touchpad;
    case Headset:
        return DeviceKind::Headset;
    case Speakers:
        return DeviceKind::Speaker;
    case Headphones:
    case OtherAudio:
        return DeviceKind::Headphones;
    }
    return DeviceKind::Other;
}

// Some receivers (e.g. Logitech Lightspeed) make UPower report a mouse as a keyboard,
// so an explicit word in the model name wins over the reported type.
DeviceKind kindOf(const QString &model, uint upowerType)
{
    if (model.contains(QLatin1String("mouse"), Qt::CaseInsensitive)) {
        return DeviceKind::Mouse;
    }
    if (model.contains(QLatin1String("keyboard"), Qt::CaseInsensitive)) {
        return DeviceKind::Keyboard;
    }
    return kindFromUPowerType(upowerType);
}

// UPower "BatteryLevel". Anything but None (1) means the device reports only a rough level
// and UPower's "Percentage" is an approximation of it; Unknown (0) and the deprecated values
// are treated like None, i.e. we fall back to the percentage.
CoarseLevel coarseLevelOf(const QVariantMap &properties)
{
    switch (properties.value(QStringLiteral("BatteryLevel")).toUInt()) {
    case 3:
        return CoarseLevel::Low;
    case 4:
        return CoarseLevel::Critical;
    case 6:
        return CoarseLevel::Normal;
    case 7:
        return CoarseLevel::High;
    case 8:
        return CoarseLevel::Full;
    }
    return CoarseLevel::None;
}

// Our stand-in level for a coarse one: fills the ring and orders the devices.
int percentageFor(CoarseLevel level)
{
    switch (level) {
    case CoarseLevel::Critical:
        return 5;
    case CoarseLevel::Low:
        return 15;
    case CoarseLevel::Normal:
        return 50;
    case CoarseLevel::High:
        return 80;
    case CoarseLevel::Full:
        return 100;
    case CoarseLevel::None:
        break;
    }
    return -1;
}

// UPower publishes a new device before it has read the battery, with a placeholder 0 %
// and an unknown state. Show that as "unknown" rather than as an empty battery.
int percentageOf(const QVariantMap &properties)
{
    if (const CoarseLevel level = coarseLevelOf(properties); level != CoarseLevel::None) {
        return percentageFor(level);
    }
    const double percentage = properties.value(QStringLiteral("Percentage")).toDouble();
    if (percentage == 0 && properties.value(QStringLiteral("State")).toUInt() == StateUnknown) {
        return -1;
    }
    return qRound(percentage);
}
}

QString UPower::deviceId(const QString &path)
{
    return QStringLiteral("upower:") + path;
}

bool UPower::isPeripheral(const QVariantMap &properties)
{
    return !properties.value(QStringLiteral("PowerSupply")).toBool();
}

bool UPower::isPresent(const QVariantMap &properties)
{
    return properties.value(QStringLiteral("IsPresent"), true).toBool();
}

Device UPower::toDevice(const QString &path, const QVariantMap &properties)
{
    const QString model = properties.value(QStringLiteral("Model")).toString();
    return {
        .id = deviceId(path),
        .address = bluetoothAddressIn(properties.value(QStringLiteral("NativePath")).toString()),
        .name = model.isEmpty() ? properties.value(QStringLiteral("Vendor")).toString() : model,
        .kind = kindOf(model, properties.value(QStringLiteral("Type")).toUInt()),
        .percentage = percentageOf(properties),
        .coarseLevel = coarseLevelOf(properties),
        .charging = properties.value(QStringLiteral("State")).toUInt() == StateCharging,
        .parts = {},
    };
}
