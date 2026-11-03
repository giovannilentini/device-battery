// SPDX-License-Identifier: MIT
#include "devicemodel.h"

DeviceModel::DeviceModel(QObject *parent)
    : DeviceListModel(parent)
{
    connect(&m_upower, &DeviceSource::devicesChanged, this, &DeviceModel::sync);
    connect(&m_airPods, &DeviceSource::devicesChanged, this, &DeviceModel::sync);
}

void DeviceModel::sync()
{
    // AirPods data is richer (per earbud and case), so it wins over a UPower entry
    // for the same Bluetooth address, e.g. when PipeWire also reports a headset level.
    setDevices(mergeDevices(m_airPods.devices(), m_upower.devices()));
}
