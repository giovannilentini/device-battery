// SPDX-License-Identifier: MIT
#include "device.h"

#include <QCollator>
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
    return QStringLiteral("preferences-system-bluetooth-battery");
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

    auto isBud = [](const BatteryPart &part) {
        return part.component != Component::Case;
    };
    QList<BatteryPart> buds;
    std::ranges::copy_if(parts, std::back_inserter(buds), isBud);
    const QList<BatteryPart> &relevant = buds.isEmpty() ? parts : buds;

    if (relevant.isEmpty()) {
        device.percentage = -1;
        device.charging = false;
        return;
    }
    const auto emptiest = std::ranges::min_element(relevant, {}, &BatteryPart::percentage);
    device.percentage = emptiest->percentage;
    device.charging = std::ranges::any_of(relevant, &BatteryPart::charging);
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
