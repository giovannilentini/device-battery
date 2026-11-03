// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QString>

// What kind of peripheral a battery belongs to. Only used to pick an icon.
enum class DeviceKind {
    Other,
    Earbuds,
    Headphones,
    Headset,
    Speaker,
    Mouse,
    Keyboard,
    Touchpad,
    Gamepad,
    Tablet,
    Phone,
    MediaPlayer,
};

// One independently powered component of a device, e.g. the left AirPod or the case.
struct BatteryPart {
    enum class Component {
        Main,
        Left,
        Right,
        Case
    };

    Component component = Component::Main;
    int percentage = -1; // 0-100, or -1 when unknown
    bool charging = false;

    bool operator==(const BatteryPart &) const = default;
};

// A device as shown in the widget, regardless of where its data came from.
struct Device {
    QString id; // unique across all sources, e.g. "upower:/org/freedesktop/..."
    QString address; // Bluetooth MAC address (upper case) if known; used to merge duplicates
    QString name;
    DeviceKind kind = DeviceKind::Other;
    int percentage = -1; // overall level, 0-100, or -1 when unknown
    bool charging = false;
    QList<BatteryPart> parts; // empty for single-battery devices

    bool operator==(const Device &) const = default;
};

// Freedesktop icon name for a device kind.
QString iconName(DeviceKind kind);

// Short, translatable-in-QML key for a part ("left", "right", "case", "main").
QString componentKey(BatteryPart::Component component);

// Fills kind, percentage, charging and parts of `device` from the parts reported by
// AirPods/Beats. The overall level is the emptiest earbud that is in use, because that is
// the one that runs out first; when both are docked we fall back to the case.
void applyBatteryParts(Device &device, QList<BatteryPart> parts);

// Extracts a Bluetooth MAC address from strings such as "/org/bluez/hci0/dev_38_C4_3A_7B_06_85"
// or "hid-38:c4:3a:7b:06:85-battery". Returns an empty string if there is none.
QString bluetoothAddressIn(const QString &text);

// Combines the devices of all sources into display order (by name, then id).
// A device in `preferred` hides any device in `fallback` with the same Bluetooth address.
QList<Device> mergeDevices(const QList<Device> &preferred, const QList<Device> &fallback);
