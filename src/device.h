// SPDX-License-Identifier: MIT
#pragma once

#include <QList>
#include <QSet>
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
    // Added later: new kinds go at the end, as the cache of disconnected devices stores the number.
    Computer,
    Modem,
    Network,
    Camera,
    Video,
    Monitor,
    Printer,
    Scanner,
    Toy,
    BluetoothGeneric,
};
constexpr DeviceKind LastDeviceKind = DeviceKind::BluetoothGeneric;

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

// Where an earbud is, as AirPods report it (in-ear detection).
enum class Placement {
    Unknown, // not reported, e.g. in-ear detection is off or the model has none
    InEar,
    OutOfEar,
    InCase,
};

// The noise control mode of AirPods and Beats that have one. Only shown, never changed.
enum class NoiseControl {
    Unknown, // not reported, e.g. the model has no noise control
    Off,
    Cancellation,
    Transparency,
    Adaptive,
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
    Placement placement = Placement::Unknown; // earbuds only

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
    QString model; // product name if known and not just the device name, e.g. "AirPods Pro (2nd generation)"
    NoiseControl noiseControl = NoiseControl::Unknown;
    bool connected = true; // false for a recently disconnected device (see updateRecentDevices())
    qint64 lastSeen = 0; // for a disconnected device: when it was last seen, in ms since the epoch

    bool operator==(const Device &) const = default;
};

// Freedesktop icon name for a device kind.
QString iconName(DeviceKind kind);

// Short, translatable-in-QML key for a part ("left", "right", "case", "main").
QString componentKey(BatteryPart::Component component);

// Short, translatable-in-QML key for a coarse level ("critical", "low", "normal", "high",
// "full"), or an empty string for CoarseLevel::None.
QString coarseLevelKey(CoarseLevel level);

// Short, translatable-in-QML key for a placement ("ear", "out", "case"), or an empty string
// for Placement::Unknown.
QString placementKey(Placement placement);

// Short, translatable-in-QML key for a noise control mode ("off", "cancellation",
// "transparency", "adaptive"), or an empty string for NoiseControl::Unknown.
QString noiseControlKey(NoiseControl mode);

// The product name of an Apple or Beats audio device from its BlueZ "Modalias", e.g.
// "bluetooth:v004Cp2014d0B5A" -> "AirPods Pro (2nd generation)". Empty if the product is unknown.
QString appleModelName(const QString &modalias);

// Sets the placement of the earbuds in `parts` from an in-ear detection report. AirPods report
// it for the primary and the secondary earbud rather than for left and right; the primary
// earbud is the one listed first in their battery reports, so `parts` must still be in the
// order of the report. Other parts are left alone.
void applyPlacements(QList<BatteryPart> &parts, Placement primary, Placement secondary);

// Fills kind, percentage, charging and parts of `device` from the parts reported by
// AirPods/Beats. The overall level is that of the emptiest earbud in use, because that is
// the one that runs out first. An earbud is in use if it is in an ear or, without in-ear
// detection, if it is not charging in the case. With no earbud in use it is the emptiest
// earbud, and with no earbud reported the case. Parts without a known level are skipped.
// The device counts as charging only if the part shown is.
void applyBatteryParts(Device &device, QList<BatteryPart> parts);

// Index of the device closest to running out, or -1 if no connected device has a known level.
// Discharging devices come first: a charging device is picked only if all of them charge.
// Disconnected devices never count.
int lowestDeviceIndex(const QList<Device> &devices);

// Extracts a Bluetooth MAC address from strings such as "/org/bluez/hci0/dev_38_C4_3A_7B_06_85"
// or "hid-38:c4:3a:7b:06:85-battery". Returns an empty string if there is none.
QString bluetoothAddressIn(const QString &text);

// A key that identifies a device across reconnections and restarts, for per-device settings:
// "bt:<address>" for Bluetooth devices, else "name:<name>". UPower ids are not stable, e.g.
// Logitech receivers recreate their object under a new path when the mouse wakes up.
QString deviceKey(const Device &device);

// Whether `a` and `b` are the same physical device, possibly under another id: same id, same
// Bluetooth address, or same name and kind.
bool isSameDevice(const Device &a, const Device &b);

// Most recently disconnected devices that are kept, with their last levels.
constexpr int MaxRecentDevices = 8;

// The recently disconnected devices after `gone` disappeared at `now` (ms since the epoch)
// while `current` are connected: `gone` are added as disconnected, with their last levels
// but no longer charging, devices that are connected again are dropped, and only the
// MaxRecentDevices most recent are kept, the most recent first.
QList<Device> updateRecentDevices(const QList<Device> &recent, const QList<Device> &gone, const QList<Device> &current, qint64 now);

// Recently disconnected devices to and from the cache file, so they survive a restart.
// Reading skips anything malformed; a file that is not valid JSON gives an empty list.
QByteArray recentDevicesToJson(const QList<Device> &recent);
QList<Device> recentDevicesFromJson(const QByteArray &json);

// The devices of `shown` that are missing from `next` and that no device of `next` takes the
// place of (same Bluetooth address, or same name and kind). Used to keep a vanished device on
// screen for a moment: e.g. Logitech receivers recreate their UPower object, under a new path,
// when the mouse wakes up, which would otherwise make the row (and the tray icon) blink.
QList<Device> vanishedDevices(const QList<Device> &shown, const QList<Device> &next);

// The devices of `devices` to warn about because their battery ran low: connected, not
// charging, with a known level at or below `threshold`, and not warned about yet. `warned`
// holds the ids already warned about and is updated: a device is warned about again only
// after it charged or rose above the threshold, or disconnected.
QList<Device> devicesToWarn(const QList<Device> &devices, QSet<QString> &warned, int threshold);

// A connected device that reports no battery level to Linux, e.g. a wireless receiver whose
// keyboard reports its level only through the vendor's own app. Listed so that a device the
// widget cannot show is not silently missing.
struct SilentDevice {
    QString key; // "bt:<address>" (as deviceKey()) or "usb:<vendor>:<product>"
    QString name;
    QString iconName;
    bool audio = false; // a Bluetooth headset or headphones (see the hint in the popup)
    bool usb = false; // a USB wireless receiver

    bool operator==(const SilentDevice &) const = default;
};

// The devices of `connected` that are not among the connected devices of `shown`, i.e. that
// have no battery level from any source, sorted by name.
QList<SilentDevice> silentDevices(const QList<SilentDevice> &connected, const QList<Device> &shown);

// Combines the devices of all sources into display order (by name, then id).
// A device in `preferred` hides any device in `fallback` with the same Bluetooth address.
QList<Device> mergeDevices(const QList<Device> &preferred, const QList<Device> &fallback);
