// SPDX-License-Identifier: MIT
#include "upowersource.h"

#include "dbusutils.h"
#include "upowerdevice.h"

#include <QDBusServiceWatcher>

namespace
{
const QString Service = QStringLiteral("org.freedesktop.UPower");
const QString ManagerPath = QStringLiteral("/org/freedesktop/UPower");
const QString ManagerInterface = QStringLiteral("org.freedesktop.UPower");
const QString DeviceInterface = QStringLiteral("org.freedesktop.UPower.Device");
}

UPowerSource::UPowerSource(const QDBusConnection &bus, QObject *parent)
    : DeviceSource(parent)
    , m_bus(bus)
{
    m_bus.connect(Service, ManagerPath, ManagerInterface, QStringLiteral("DeviceAdded"), this, SLOT(onDeviceAdded(QDBusObjectPath)));
    m_bus.connect(Service, ManagerPath, ManagerInterface, QStringLiteral("DeviceRemoved"), this, SLOT(onDeviceRemoved(QDBusObjectPath)));

    // upowerd sends no DeviceRemoved when it exits (update, crash), so without this we would
    // keep showing frozen levels until plasmashell restarts. Also covers a late start.
    auto *watcher = new QDBusServiceWatcher(Service, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
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
    DBus::callAsync(m_bus, enumerate, this, [this](const QDBusMessage &reply) {
        for (const QDBusObjectPath &path : DBus::argument<QList<QDBusObjectPath>>(reply, 0)) {
            onDeviceAdded(path);
        }
    });
}

void UPowerSource::reset()
{
    const QSet<QString> watched = m_watched; // unwatch() modifies m_watched
    for (const QString &path : watched) {
        unwatch(path);
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
    DBus::callAsync(m_bus, getAll, this, [this, path](const QDBusMessage &reply) {
        track(path, DBus::argument<QVariantMap>(reply, 0));
    });
}

void UPowerSource::onDeviceRemoved(const QDBusObjectPath &objectPath)
{
    const QString path = objectPath.path();
    unwatch(path);
    if (m_properties.remove(path)) {
        remove(UPower::deviceId(path));
    }
}

void UPowerSource::track(const QString &path, const QVariantMap &properties)
{
    if (!UPower::isPeripheral(properties)) {
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
    if (m_watched.contains(path)) {
        return;
    }
    m_watched.insert(path);
    m_bus.connect(Service, path, DBus::PropertiesInterface, QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QDBusMessage)));
}

void UPowerSource::unwatch(const QString &path)
{
    if (!m_watched.remove(path)) {
        return;
    }
    m_bus.disconnect(Service, path, DBus::PropertiesInterface, QStringLiteral("PropertiesChanged"), this, SLOT(onPropertiesChanged(QDBusMessage)));
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
    const QVariantMap properties = m_properties.value(path);
    // A receiver can stay plugged in while its mouse is switched off.
    if (UPower::isPresent(properties)) {
        upsert(UPower::toDevice(path, properties));
    } else {
        remove(UPower::deviceId(path));
    }
}
