// SPDX-License-Identifier: MIT
#pragma once

#include "airpodssource.h"
#include "devicelistmodel.h"
#include "upowersource.h"

#include <QQmlEngine>

// Every device with a battery, merged from all sources: the model the widget shows.
//
// A QML singleton, so all widget instances (panel, system tray, desktop) share one
// backend and one AAP connection per device.
class DeviceModel : public DeviceListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit DeviceModel(QObject *parent = nullptr);

private:
    void sync();

    UPowerSource m_upower;
    AirPodsSource m_airPods;
};
