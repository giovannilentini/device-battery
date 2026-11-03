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

private:
    void sync();
    void syncNow(); // without keeping vanished devices around

    std::unique_ptr<UPowerSource> m_upower;
    std::unique_ptr<AirPodsSource> m_airPods;
    int m_users = 0;
    QTimer m_graceTimer; // runs while vanished devices are still shown, see sync()
};
