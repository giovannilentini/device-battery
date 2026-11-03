// SPDX-License-Identifier: MIT
#pragma once

#include "devicesource.h"

#include <QDBusMessage>
#include <QVariantMap>

class AapSession;

// AirPods and Beats, whose battery BlueZ cannot read.
//
// Watches BlueZ for connected Apple audio devices and opens one AapSession per device,
// which reports left earbud, right earbud and case separately. Sessions exist only
// while a device is connected, so nothing runs when no AirPods are around.
class AirPodsSource : public DeviceSource
{
    Q_OBJECT

public:
    explicit AirPodsSource(QObject *parent = nullptr);

private Q_SLOTS:
    void onInterfacesAdded(const QDBusMessage &message);
    void onInterfacesRemoved(const QDBusMessage &message);
    void onPropertiesChanged(const QDBusMessage &message);

private:
    struct Tracked {
        QString address;
        QString name;
        AapSession *session = nullptr;
        QList<BatteryPart> parts; // last report, kept so a rename can be published
    };

    void addDevice(const QString &path, const QVariantMap &properties);
    void evaluate(const QString &path);
    void start(const QString &path, const QVariantMap &properties);
    void stop(const QString &path);
    void publish(const Tracked &tracked);

    QHash<QString, QVariantMap> m_properties; // BlueZ device path -> org.bluez.Device1 properties
    QHash<QString, Tracked> m_tracked; // BlueZ device path -> active AAP session
};
