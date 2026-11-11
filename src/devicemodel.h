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

    // Connected devices that report no battery level, so they are not shown as devices:
    // [{key, name, iconName, audio, usb}, ...] (see SilentDevice).
    Q_PROPERTY(QVariantList silentDevices READ silentDevicesVariant NOTIFY silentDevicesChanged)

public:
    explicit DeviceModel(QObject *parent = nullptr);
    ~DeviceModel() override;

    Q_INVOKABLE void acquire();
    Q_INVOKABLE void release();

    QVariantList silentDevicesVariant() const;
    // Looks for USB wireless receivers without a battery again. Cheap, but it reads sysfs, so
    // the popup calls it when it opens rather than on a timer.
    Q_INVOKABLE void refreshSilentDevices();

    // Drops the recently disconnected devices, also from the cache file.
    Q_INVOKABLE void forgetDisconnected();

    // Low battery notifications for AirPods and Beats. UPower devices are left out: Plasma's
    // power management already notifies about them. The texts come from QML, which has the
    // widget's translations: `appName` is the widget's name, in `title` %1 is the device name,
    // in `body` %1 the level.
    // Every widget instance calls this with its own id and settings. The backend is shared and
    // notifies once per device: if any instance has notifications on, at the highest threshold
    // among those that do (see notificationSettings()).
    Q_INVOKABLE void configureNotifications(int instance, bool enabled, int threshold, const QString &appName, const QString &title, const QString &body);
    // Forgets the settings of a widget instance that is being removed.
    Q_INVOKABLE void removeNotifications(int instance);

Q_SIGNALS:
    void silentDevicesChanged();

private:
    void sync();
    void updateSilentDevices();
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
    QHash<int, NotificationSetting> m_notifySettings; // widget instance id -> its settings
    QString m_notifyAppName;
    QString m_notifyTitle;
    QString m_notifyBody;
    QSet<QString> m_warned; // see devicesToWarn()

    QList<SilentDevice> m_usbReceivers; // from the last refreshSilentDevices()
    QList<SilentDevice> m_silent;
};
