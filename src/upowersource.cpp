// SPDX-License-Identifier: MIT
#include "upowersource.h"

#include "dbusutils.h"

#include <QDBusServiceWatcher>

namespace
{
const QString Service = QStringLiteral("org.freedesktop.UPower");
const QString ManagerPath = QStringLiteral("/org/freedesktop/UPower");
const QString ManagerInterface = QStringLiteral("org.freedesktop.UPower");
const QString DeviceInterface = QStringLiteral("org.freedesktop.UPower.Device");

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

QString deviceId(const QString &path)
{
    return QStringLiteral("upower:") + path;
}

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

// Laptop batteries, UPSes and AC adapters power the computer itself; skip them.
bool isPeripheral(const QVariantMap &properties)
{
    return !properties.value(QStringLiteral("PowerSupply")).toBool();
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

// UPower publishes a new device before it has read the battery, with a placeholder 0 %
// and an unknown state. Show that as "unknown" rather than as an empty battery.
int percentageOf(const QVariantMap &properties)
{
    const double percentage = properties.value(QStringLiteral("Percentage")).toDouble();
    if (percentage == 0 && properties.value(QStringLiteral("State")).toUInt() == StateUnknown) {
        return -1;
    }
    return qRound(percentage);
}

Device toDevice(const QString &path, const QVariantMap &properties)
{
    const QString model = properties.value(QStringLiteral("Model")).toString();
    return {
        .id = deviceId(path),
        .address = bluetoothAddressIn(properties.value(QStringLiteral("NativePath")).toString()),
        .name = model.isEmpty() ? properties.value(QStringLiteral("Vendor")).toString() : model,
        .kind = kindOf(model, properties.value(QStringLiteral("Type")).toUInt()),
        .percentage = percentageOf(properties),
        .charging = properties.value(QStringLiteral("State")).toUInt() == StateCharging,
        .parts = {},
    };
}
}

UPowerSource::UPowerSource(QObject *parent)
    : DeviceSource(parent)
{
    QDBusConnection bus = QDBusConnection::systemBus();
    bus.connect(Service, ManagerPath, ManagerInterface, QStringLiteral("DeviceAdded"), this, SLOT(onDeviceAdded(QDBusObjectPath)));
    bus.connect(Service, ManagerPath, ManagerInterface, QStringLiteral("DeviceRemoved"), this, SLOT(onDeviceRemoved(QDBusObjectPath)));

    // upowerd sends no DeviceRemoved when it exits (update, crash), so without this we would
    // keep showing frozen levels until plasmashell restarts. Also covers a late start.
    auto *watcher = new QDBusServiceWatcher(Service, bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        qCDebug(DEVICEBATTERY) << "UPower" << (newOwner.isEmpty() ? "went away" : "(re)started");
        reset();
        if (!newOwner.isEmpty()) {
            enumerate();
        }
    });

    enumerate();
}

void UPowerSource::enumerate()
{
    const auto enumerate = QDBusMessage::createMethodCall(Service, ManagerPath, ManagerInterface, QStringLiteral("EnumerateDevices"));
    DBus::callAsync(QDBusConnection::systemBus(), enumerate, this, [this](const QDBusMessage &reply) {
        for (const QDBusObjectPath &path : DBus::argument<QList<QDBusObjectPath>>(reply, 0)) {
            onDeviceAdded(path);
        }
    });
}

void UPowerSource::reset()
{
    for (auto it = m_properties.cbegin(); it != m_properties.cend(); ++it) {
        unwatch(it.key());
    }
    m_properties.clear();
    clear();
}

void UPowerSource::onDeviceAdded(const QDBusObjectPath &objectPath)
{
    const QString path = objectPath.path();

    // Subscribe *before* reading the properties. UPower often fills in the real level right
    // after announcing a device; subscribing only after our read returned would miss that
    // update and leave the placeholder value on screen.
    watch(path);

    auto getAll = QDBusMessage::createMethodCall(Service, path, DBus::PropertiesInterface, QStringLiteral("GetAll"));
    getAll << DeviceInterface;
    DBus::callAsync(QDBusConnection::systemBus(), getAll, this, [this, path](const QDBusMessage &reply) {
        track(path, DBus::argument<QVariantMap>(reply, 0));
    });
}

void UPowerSource::onDeviceRemoved(const QDBusObjectPath &objectPath)
{
    const QString path = objectPath.path();
    unwatch(path);
    if (m_properties.remove(path)) {
        remove(deviceId(path));
    }
}

void UPowerSource::track(const QString &path, const QVariantMap &properties)
{
    if (!isPeripheral(properties)) {
        unwatch(path); // the laptop battery updates often and is not ours to show
        return;
    }
    if (!m_properties.contains(path)) {
        m_properties.insert(path, properties);
        publish(path);
    }
}

void UPowerSource::watch(const QString &path)
{
    QDBusConnection::systemBus().connect(Service, path, DBus::PropertiesInterface, QStringLiteral("PropertiesChanged"), this,
                                         SLOT(onPropertiesChanged(QDBusMessage)));
}

void UPowerSource::unwatch(const QString &path)
{
    QDBusConnection::systemBus().disconnect(Service, path, DBus::PropertiesInterface, QStringLiteral("PropertiesChanged"), this,
                                            SLOT(onPropertiesChanged(QDBusMessage)));
}

void UPowerSource::onPropertiesChanged(const QDBusMessage &message)
{
    const auto cached = m_properties.find(message.path());
    if (cached == m_properties.end() || DBus::argument<QString>(message, 0) != DeviceInterface) {
        return;
    }
    cached->insert(DBus::argument<QVariantMap>(message, 1));
    publish(message.path());
}

void UPowerSource::publish(const QString &path)
{
    const QVariantMap &properties = m_properties.value(path);
    // A receiver can stay plugged in while its mouse is switched off.
    if (properties.value(QStringLiteral("IsPresent"), true).toBool()) {
        upsert(toDevice(path, properties));
    } else {
        remove(deviceId(path));
    }
}
