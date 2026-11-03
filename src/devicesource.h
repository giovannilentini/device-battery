// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QHash>
#include <QObject>

// Base class for anything that discovers devices and their battery levels.
// Subclasses call upsert()/remove(); listeners only need devices() and devicesChanged().
class DeviceSource : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;

    QList<Device> devices() const
    {
        return m_devices.values();
    }

Q_SIGNALS:
    void devicesChanged();

protected:
    // Adds or replaces a device; does nothing (and emits nothing) if it did not change.
    void upsert(const Device &device)
    {
        const auto existing = m_devices.constFind(device.id);
        if (existing != m_devices.constEnd() && *existing == device) {
            return;
        }
        m_devices.insert(device.id, device);
        Q_EMIT devicesChanged();
    }

    void remove(const QString &id)
    {
        if (m_devices.remove(id)) {
            Q_EMIT devicesChanged();
        }
    }

    // Forgets every device at once, e.g. when the service providing them went away.
    void clear()
    {
        if (!m_devices.isEmpty()) {
            m_devices.clear();
            Q_EMIT devicesChanged();
        }
    }

private:
    QHash<QString, Device> m_devices;
};
