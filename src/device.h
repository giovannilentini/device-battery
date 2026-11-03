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

// For devices that report only a rough level instead of a percentage (UPower "BatteryLevel",
// e.g. many older Logitech mice). Their `percentage` is then our own stand-in for that level,
// good for filling a ring and for ordering, but the UI shows the word, not the number.
enum class CoarseLevel {
    None, // the device reports a real percentage
    Critical,
    Low,
    Normal,
    High,
    Full,
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
    CoarseLevel coarseLevel = CoarseLevel::None;
    bool charging = false;
    QList<BatteryPart> parts; // empty for single-battery devices

    bool operator==(const Device &) const = default;
};

// Freedesktop icon name for a device kind.
QString iconName(DeviceKind kind);

// Short, translatable-in-QML key for a part ("left", "right", "case", "main").
QString componentKey(BatteryPart::Component component);

// Short, translatable-in-QML key for a coarse level ("critical", "low", "normal", "high",
// "full"), or an empty string for CoarseLevel::None.
QString coarseLevelKey(CoarseLevel level);

// Fills kind, percentage, charging and parts of `device` from the parts reported by
// AirPods/Beats. The overall level is that of the emptiest earbud in use (one charging in
// the case is not), because that is the one that runs out first; with both earbuds in the
// case it is the emptiest of them, and with no earbud reported the case. Parts without a
// known level are skipped. The device counts as charging only if the part shown is.
void applyBatteryParts(Device &device, QList<BatteryPart> parts);

// Index of the device closest to running out, or -1 if no device has a known level.
// Discharging devices come first: a charging device is picked only if all of them charge.
int lowestDeviceIndex(const QList<Device> &devices);

// Extracts a Bluetooth MAC address from strings such as "/org/bluez/hci0/dev_38_C4_3A_7B_06_85"
// or "hid-38:c4:3a:7b:06:85-battery". Returns an empty string if there is none.
QString bluetoothAddressIn(const QString &text);

// The devices of `shown` that are missing from `next` and that no device of `next` takes the
// place of (same Bluetooth address, or same name and kind). Used to keep a vanished device on
// screen for a moment: e.g. Logitech receivers recreate their UPower object, under a new path,
// when the mouse wakes up, which would otherwise make the row (and the tray icon) blink.
QList<Device> vanishedDevices(const QList<Device> &shown, const QList<Device> &next);

// Combines the devices of all sources into display order (by name, then id).
// A device in `preferred` hides any device in `fallback` with the same Bluetooth address.
QList<Device> mergeDevices(const QList<Device> &preferred, const QList<Device> &fallback);
