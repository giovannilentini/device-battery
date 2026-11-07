// SPDX-License-Identifier: MIT
#pragma once

#include "airpodssource.h"
#include "devicelistmodel.h"
#include "upowersource.h"

#include <QQmlEngine>
#include <QTimer>

#include <memory>

// Every device with a battery, merged from all sources: the model the widget shows.
//
// A QML singleton, so all widget instances (panel, system tray, desktop) share one
// backend and one AAP connection per device.
//
// The singleton belongs to plasmashell's QML engine and so outlives the widgets, which is
// why the sources only run while a widget uses them: every instance calls acquire() when it
// is created and release() when it is destroyed. Once the last one is gone, the D-Bus
// subscriptions and AAP sessions stop, so a removed widget no longer occupies the AirPods'
// single AAP connection.
//
// A device that disconnects stays listed with its last levels, below the connected ones (see
// updateRecentDevices()). These are kept in a small cache file, so they survive a restart.
class DeviceModel : public DeviceListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit DeviceModel(QObject *parent = nullptr);
    ~DeviceModel() override;

    Q_INVOKABLE void acquire();
    Q_INVOKABLE void release();

    // Low battery notifications for AirPods and Beats. UPower devices are left out: Plasma's
    // power management already notifies about them. The texts come from QML, which has the
    // widget's translations: in `title` %1 is the device name, in `body` %1 the level.
    // Every widget instance calls this; as the backend is shared, the last call wins.
    Q_INVOKABLE void configureNotifications(bool enabled, int threshold, const QString &title, const QString &body);

private:
    void sync();
    void syncNow(); // without keeping vanished devices around
    QList<Device> connectedDevices() const;
    QList<Device> withRecent(const QList<Device> &connected) const;
    void loadRecent();
    void saveRecent() const;

    std::unique_ptr<UPowerSource> m_upower;
    std::unique_ptr<AirPodsSource> m_airPods;
    int m_users = 0;
    QTimer m_graceTimer; // runs while vanished devices are still shown, see sync()
    QList<Device> m_recent; // recently disconnected devices, the most recent first

    void notifyLowBatteries();
    bool m_notify = false;
    int m_notifyThreshold = 20;
    QString m_notifyTitle;
    QString m_notifyBody;
    QSet<QString> m_warned; // see devicesToWarn()
};
