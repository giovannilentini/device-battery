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

    // The part whose level we show: the emptiest earbud in use, i.e. not charging in the case;
    // with both earbuds in the case, the emptiest of them; with no earbud reported, the case.
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
    const BatteryPart *shown = emptiest([](const BatteryPart &part) {
        return part.component != Component::Case && !part.charging;
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
        if (device.percentage < 0) {
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

QList<Device> vanishedDevices(const QList<Device> &shown, const QList<Device> &next)
{
    QList<Device> vanished;
    for (const Device &old : shown) {
        const bool replaced = std::ranges::any_of(next, [&old](const Device &device) {
            return device.id == old.id || (!old.address.isEmpty() && device.address == old.address) || (device.name == old.name && device.kind == old.kind);
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
