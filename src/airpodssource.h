// SPDX-License-Identifier: MIT
#pragma once

#include "devicesource.h"

#include <QDBusConnection>
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
    // `bus` is the system bus in the widget; tests pass a private bus with a fake BlueZ.
    explicit AirPodsSource(const QDBusConnection &bus = QDBusConnection::systemBus(), QObject *parent = nullptr);

    // Every connected Bluetooth device except the AirPods this source reads, whether or not it
    // has a battery: the widget lists the ones nothing reports a level for. BlueZ is watched
    // for AirPods anyway, so this costs no extra D-Bus traffic.
    QList<SilentDevice> silentDevices() const;

Q_SIGNALS:
    // The connected Bluetooth devices or their names changed (see silentDevices()).
    void connectionsChanged();

private Q_SLOTS:
    void onInterfacesAdded(const QDBusMessage &message);
    void onInterfacesRemoved(const QDBusMessage &message);
    void onPropertiesChanged(const QDBusMessage &message);

private:
    struct Tracked {
        QString address;
        QString name;
        AapSession *session = nullptr;
        QList<BatteryPart> parts; // last report, in its order (see applyPlacements())
        QString model;
        Placement primary = Placement::Unknown;
        Placement secondary = Placement::Unknown;
        NoiseControl noiseControl = NoiseControl::Unknown;
    };

    void enumerate();
    void reset();
    void addDevice(const QString &path, const QVariantMap &properties);
    void evaluate(const QString &path);
    void evaluateAirPods(const QString &path);
    void start(const QString &path, const QVariantMap &properties);
    void stop(const QString &path);
    void publish(const Tracked &tracked);

    QDBusConnection m_bus;
    // BlueZ device path -> the org.bluez.Device1 properties we use (see RelevantKeys). Every
    // BlueZ device is cached, also ones merely discovered nearby, so only a few keys are kept.
    QHash<QString, QVariantMap> m_properties;
    QHash<QString, Tracked> m_tracked; // BlueZ device path -> active AAP session
};
