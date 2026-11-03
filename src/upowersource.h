// SPDX-License-Identifier: MIT
#pragma once

#include "devicesource.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusObjectPath>
#include <QSet>
#include <QVariantMap>

// Peripheral batteries known to UPower: Bluetooth devices that BlueZ reports a level for
// (BLE keyboards, mice, many headsets) and kernel HID batteries such as Logitech
// receivers. The laptop's own battery and AC adapter are ignored.
//
// Fully event-driven: we cache each device's properties and apply the deltas that UPower
// pushes, so there is no polling and no extra D-Bus round trip per update.
class UPowerSource : public DeviceSource
{
    Q_OBJECT

public:
    // `bus` is the system bus in the widget; tests pass a private bus with a fake UPower.
    explicit UPowerSource(const QDBusConnection &bus = QDBusConnection::systemBus(), QObject *parent = nullptr);

private Q_SLOTS:
    void onDeviceAdded(const QDBusObjectPath &objectPath);
    void onDeviceRemoved(const QDBusObjectPath &objectPath);
    void onPropertiesChanged(const QDBusMessage &message);

private:
    void enumerate();
    void reset();
    void track(const QString &path, const QVariantMap &properties);
    void publish(const QString &path);
    void watch(const QString &path);
    void unwatch(const QString &path);

    QDBusConnection m_bus;
    QHash<QString, QVariantMap> m_properties; // peripheral object path -> UPower properties
    // Paths subscribed to, including those whose GetAll has not returned (or failed), so
    // that reset() can drop every subscription, not only those of tracked devices.
    QSet<QString> m_watched;
};
