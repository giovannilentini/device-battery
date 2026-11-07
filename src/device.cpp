// SPDX-License-Identifier: MIT
#include "device.h"

#include <QCollator>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QRegularExpression>
#include <QSet>

#include <algorithm>

QString iconName(DeviceKind kind)
{
    switch (kind) {
    case DeviceKind::Earbuds:
    case DeviceKind::Headphones:
        return QStringLiteral("audio-headphones");
    case DeviceKind::Headset:
        return QStringLiteral("audio-headset");
    case DeviceKind::Speaker:
        return QStringLiteral("audio-speakers");
    case DeviceKind::Mouse:
        return QStringLiteral("input-mouse");
    case DeviceKind::Keyboard:
        return QStringLiteral("input-keyboard");
    case DeviceKind::Touchpad:
        return QStringLiteral("input-touchpad");
    case DeviceKind::Gamepad:
        return QStringLiteral("input-gaming");
    case DeviceKind::Tablet:
        return QStringLiteral("input-tablet");
    case DeviceKind::Phone:
        return QStringLiteral("phone");
    case DeviceKind::MediaPlayer:
        return QStringLiteral("multimedia-player");
    case DeviceKind::Other:
        break;
    }
    return QStringLiteral("preferences-system-bluetooth-battery-symbolic");
}

QString componentKey(BatteryPart::Component component)
{
    switch (component) {
    case BatteryPart::Component::Left:
        return QStringLiteral("left");
    case BatteryPart::Component::Right:
        return QStringLiteral("right");
    case BatteryPart::Component::Case:
        return QStringLiteral("case");
    case BatteryPart::Component::Main:
        break;
    }
    return QStringLiteral("main");
}

QString coarseLevelKey(CoarseLevel level)
{
    switch (level) {
    case CoarseLevel::Critical:
        return QStringLiteral("critical");
    case CoarseLevel::Low:
        return QStringLiteral("low");
    case CoarseLevel::Normal:
        return QStringLiteral("normal");
    case CoarseLevel::High:
        return QStringLiteral("high");
    case CoarseLevel::Full:
        return QStringLiteral("full");
    case CoarseLevel::None:
        break;
    }
    return {};
}

QString placementKey(Placement placement)
{
    switch (placement) {
    case Placement::InEar:
        return QStringLiteral("ear");
    case Placement::OutOfEar:
        return QStringLiteral("out");
    case Placement::InCase:
        return QStringLiteral("case");
    case Placement::Unknown:
        break;
    }
    return {};
}

QString noiseControlKey(NoiseControl mode)
{
    switch (mode) {
    case NoiseControl::Off:
        return QStringLiteral("off");
    case NoiseControl::Cancellation:
        return QStringLiteral("cancellation");
    case NoiseControl::Transparency:
        return QStringLiteral("transparency");
    case NoiseControl::Adaptive:
        return QStringLiteral("adaptive");
    case NoiseControl::Unknown:
        break;
    }
    return {};
}

QString appleModelName(const QString &modalias)
{
    // "bluetooth:v004Cp2014d0B5A": Apple's vendor ID 0x004C, then the product ID.
    static const QRegularExpression pattern(QStringLiteral("^bluetooth:v004Cp([0-9A-F]{4})"), QRegularExpression::CaseInsensitiveOption);
    const QRegularExpressionMatch match = pattern.match(modalias);
    if (!match.hasMatch()) {
        return {};
    }
    // Product names are proper names, so they are not translated.
    static const QHash<quint16, QString> names{
        {0x2002, QStringLiteral("AirPods (1st generation)")},
        {0x200F, QStringLiteral("AirPods (2nd generation)")},
        {0x2013, QStringLiteral("AirPods (3rd generation)")},
        {0x2019, QStringLiteral("AirPods 4")},
        {0x201B, QStringLiteral("AirPods 4 (ANC)")},
        {0x200E, QStringLiteral("AirPods Pro")},
        {0x2014, QStringLiteral("AirPods Pro (2nd generation)")},
        {0x2024, QStringLiteral("AirPods Pro (2nd generation, USB-C)")},
        {0x200A, QStringLiteral("AirPods Max")},
        {0x2003, QStringLiteral("Powerbeats3")},
        {0x2005, QStringLiteral("BeatsX")},
        {0x2006, QStringLiteral("Beats Solo3")},
        {0x2009, QStringLiteral("Beats Studio3")},
        {0x200B, QStringLiteral("Powerbeats Pro")},
        {0x200C, QStringLiteral("Beats Solo Pro")},
        {0x200D, QStringLiteral("Powerbeats")},
        {0x2010, QStringLiteral("Beats Flex")},
        {0x2011, QStringLiteral("Beats Studio Buds")},
        {0x2012, QStringLiteral("Beats Fit Pro")},
        {0x2016, QStringLiteral("Beats Studio Buds+")},
        {0x2017, QStringLiteral("Beats Studio Pro")},
    };
    return names.value(match.captured(1).toUShort(nullptr, 16));
}

void applyPlacements(QList<BatteryPart> &parts, Placement primary, Placement secondary)
{
    bool primaryDone = false;
    for (BatteryPart &part : parts) {
        if (part.component != BatteryPart::Component::Left && part.component != BatteryPart::Component::Right) {
            continue;
        }
        part.placement = primaryDone ? secondary : primary;
        if (primaryDone) {
            return;
        }
        primaryDone = true;
    }
}

void applyBatteryParts(Device &device, QList<BatteryPart> parts)
{
    using Component = BatteryPart::Component;

    // Over-ear models (AirPods Max, most Beats) have a single battery: no parts to show.
    const auto main = std::ranges::find(parts, Component::Main, &BatteryPart::component);
    if (main != parts.end()) {
        device.kind = DeviceKind::Headphones;
        device.percentage = main->percentage;
        device.charging = main->charging;
        device.parts.clear();
        return;
    }

    std::ranges::sort(parts, {}, &BatteryPart::component); // Left, Right, Case
    device.kind = DeviceKind::Earbuds;
    device.parts = parts;

    // The part whose level we show: the emptiest earbud in use, i.e. in an ear or, without
    // in-ear detection, not charging in the case; with no earbud in use, the emptiest of them;
    // with no earbud reported, the case.
    // A part without a reading never wins, so one silent earbud cannot hide the other.
    const auto emptiest = [&parts](auto &&accept) -> const BatteryPart * {
        const BatteryPart *best = nullptr;
        for (const BatteryPart &part : std::as_const(parts)) {
            if (part.percentage >= 0 && accept(part) && (!best || part.percentage < best->percentage)) {
                best = &part;
            }
        }
        return best;
    };
    const bool earDetection = std::ranges::any_of(parts, [](const BatteryPart &part) {
        return part.placement != Placement::Unknown;
    });
    const BatteryPart *shown = emptiest([earDetection](const BatteryPart &part) {
        if (part.component == Component::Case) {
            return false;
        }
        return earDetection ? part.placement == Placement::InEar : !part.charging;
    });
    if (!shown) {
        shown = emptiest([](const BatteryPart &part) {
            return part.component != Component::Case;
        });
    }
    if (!shown) {
        shown = emptiest([](const BatteryPart &) {
            return true;
        });
    }
    device.percentage = shown ? shown->percentage : -1;
    // The state of the level we show: an earbud charging in the case must not mark the
    // device as charging while the one in the ear runs down.
    device.charging = shown && shown->charging;
}

int lowestDeviceIndex(const QList<Device> &devices)
{
    // A charging device is not running out, so it only counts when every device charges.
    int lowest = -1;
    bool lowestCharging = true;
    for (int i = 0; i < devices.size(); ++i) {
        const Device &device = devices.at(i);
        if (device.percentage < 0 || !device.connected) {
            continue;
        }
        const bool better =
            lowest < 0 || (lowestCharging && !device.charging) || (lowestCharging == device.charging && device.percentage < devices.at(lowest).percentage);
        if (better) {
            lowest = i;
            lowestCharging = device.charging;
        }
    }
    return lowest;
}

QString bluetoothAddressIn(const QString &text)
{
    static const QRegularExpression pattern(QStringLiteral("([0-9A-Fa-f]{2}[:_]){5}[0-9A-Fa-f]{2}"));
    const QRegularExpressionMatch match = pattern.match(text);
    if (!match.hasMatch()) {
        return {};
    }
    return match.captured().replace(QLatin1Char('_'), QLatin1Char(':')).toUpper();
}

bool isSameDevice(const Device &a, const Device &b)
{
    return a.id == b.id || (!a.address.isEmpty() && a.address == b.address) || (a.name == b.name && a.kind == b.kind);
}

QList<Device> updateRecentDevices(const QList<Device> &recent, const QList<Device> &gone, const QList<Device> &current, qint64 now)
{
    QList<Device> result;
    for (Device device : gone) {
        device.connected = false;
        device.lastSeen = now;
        // A last level, not a live one: nothing charges or sits in an ear any more.
        device.charging = false;
        device.noiseControl = NoiseControl::Unknown;
        for (BatteryPart &part : device.parts) {
            part.charging = false;
            part.placement = Placement::Unknown;
        }
        result.append(device);
    }
    for (const Device &device : recent) {
        const auto matches = [&device](const Device &other) {
            return isSameDevice(device, other);
        };
        if (std::ranges::none_of(result, matches) && std::ranges::none_of(current, matches)) {
            result.append(device);
        }
    }
    std::ranges::stable_sort(result, std::ranges::greater{}, &Device::lastSeen);
    if (result.size() > MaxRecentDevices) {
        result.resize(MaxRecentDevices);
    }
    return result;
}

QByteArray recentDevicesToJson(const QList<Device> &recent)
{
    QJsonArray devices;
    for (const Device &device : recent) {
        QJsonArray parts;
        for (const BatteryPart &part : device.parts) {
            parts.append(QJsonObject{
                {QStringLiteral("component"), static_cast<int>(part.component)},
                {QStringLiteral("percentage"), part.percentage},
            });
        }
        devices.append(QJsonObject{
            {QStringLiteral("id"), device.id},
            {QStringLiteral("address"), device.address},
            {QStringLiteral("name"), device.name},
            {QStringLiteral("kind"), static_cast<int>(device.kind)},
            {QStringLiteral("percentage"), device.percentage},
            {QStringLiteral("coarseLevel"), static_cast<int>(device.coarseLevel)},
            {QStringLiteral("parts"), parts},
            {QStringLiteral("model"), device.model},
            {QStringLiteral("lastSeen"), device.lastSeen},
        });
    }
    return QJsonDocument(devices).toJson(QJsonDocument::Compact);
}

QList<Device> recentDevicesFromJson(const QByteArray &json)
{
    // Enums are stored as numbers: accept only values the enum has.
    const auto inRange = [](int value, auto last) {
        return value >= 0 && value <= static_cast<int>(last);
    };
    const auto validLevel = [](int level) {
        return level >= -1 && level <= 100;
    };

    QList<Device> recent;
    for (const QJsonValue &value : QJsonDocument::fromJson(json).array()) {
        const QJsonObject object = value.toObject();
        Device device;
        device.id = object.value(QStringLiteral("id")).toString();
        device.address = object.value(QStringLiteral("address")).toString();
        device.name = object.value(QStringLiteral("name")).toString();
        device.model = object.value(QStringLiteral("model")).toString();
        device.percentage = object.value(QStringLiteral("percentage")).toInt(-1);
        device.lastSeen = object.value(QStringLiteral("lastSeen")).toInteger();
        device.connected = false;
        const int kind = object.value(QStringLiteral("kind")).toInt();
        const int coarseLevel = object.value(QStringLiteral("coarseLevel")).toInt();
        if (device.id.isEmpty() || device.lastSeen <= 0 || !validLevel(device.percentage) || !inRange(kind, DeviceKind::MediaPlayer)
            || !inRange(coarseLevel, CoarseLevel::Full)) {
            continue;
        }
        device.kind = static_cast<DeviceKind>(kind);
        device.coarseLevel = static_cast<CoarseLevel>(coarseLevel);
        for (const QJsonValue &partValue : object.value(QStringLiteral("parts")).toArray()) {
            const QJsonObject part = partValue.toObject();
            const int component = part.value(QStringLiteral("component")).toInt(-1);
            const int level = part.value(QStringLiteral("percentage")).toInt(-1);
            if (inRange(component, BatteryPart::Component::Case) && validLevel(level)) {
                BatteryPart entry;
                entry.component = static_cast<BatteryPart::Component>(component);
                entry.percentage = level;
                device.parts.append(entry);
            }
        }
        recent.append(device);
        if (recent.size() == MaxRecentDevices) {
            break;
        }
    }
    return recent;
}

QList<Device> vanishedDevices(const QList<Device> &shown, const QList<Device> &next)
{
    QList<Device> vanished;
    for (const Device &old : shown) {
        const bool replaced = std::ranges::any_of(next, [&old](const Device &device) {
            return isSameDevice(old, device);
        });
        if (!replaced) {
            vanished.append(old);
        }
    }
    return vanished;
}

QList<Device> mergeDevices(const QList<Device> &preferred, const QList<Device> &fallback)
{
    QList<Device> devices = preferred;
    QSet<QString> claimed;
    for (const Device &device : preferred) {
        if (!device.address.isEmpty()) {
            claimed.insert(device.address);
        }
    }
    for (const Device &device : fallback) {
        if (device.address.isEmpty() || !claimed.contains(device.address)) {
            devices.append(device);
        }
    }

    // The C locale (LANG=C, containers) has no collation rules, and QCollator then ignores
    // numeric mode, sorting "Mouse 10" before "Mouse 2". Use English rules there instead.
    QLocale locale;
    if (locale.language() == QLocale::C) {
        locale = QLocale(QLocale::English);
    }
    QCollator collator(locale);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    collator.setNumericMode(true);
    std::ranges::sort(devices, [&collator](const Device &a, const Device &b) {
        const int order = collator.compare(a.name, b.name);
        return order != 0 ? order < 0 : a.id < b.id;
    });
    return devices;
}
