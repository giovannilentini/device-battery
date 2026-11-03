// SPDX-License-Identifier: MIT
#include "devicemodel.h"

#include "logging.h"

#include <chrono>

namespace
{
// How long a device that disappeared stays on screen, so that one that comes right back
// (e.g. a Logitech mouse waking up, which recreates its UPower object) does not blink.
constexpr auto GracePeriod = std::chrono::seconds(2);
}

DeviceModel::DeviceModel(QObject *parent)
    : DeviceListModel(parent)
{
    m_graceTimer.setSingleShot(true);
    m_graceTimer.setInterval(GracePeriod);
    connect(&m_graceTimer, &QTimer::timeout, this, &DeviceModel::syncNow);
}

DeviceModel::~DeviceModel() = default;

void DeviceModel::acquire()
{
    if (m_users++ > 0) {
        return;
    }
    qCDebug(DEVICEBATTERY) << "First widget instance: starting the device sources";
    m_upower = std::make_unique<UPowerSource>();
    m_airPods = std::make_unique<AirPodsSource>();
    connect(m_upower.get(), &DeviceSource::devicesChanged, this, &DeviceModel::sync);
    connect(m_airPods.get(), &DeviceSource::devicesChanged, this, &DeviceModel::sync);
}

void DeviceModel::release()
{
    if (m_users == 0) {
        qCWarning(DEVICEBATTERY) << "DeviceModel::release() without acquire()";
        return;
    }
    if (--m_users > 0) {
        return;
    }
    qCDebug(DEVICEBATTERY) << "Last widget instance gone: stopping the device sources";
    m_upower.reset();
    m_airPods.reset(); // closes the AAP sessions
    syncNow();
}

void DeviceModel::sync()
{
    // AirPods data is richer (per earbud and case), so it wins over a UPower entry
    // for the same Bluetooth address, e.g. when PipeWire also reports a headset level.
    const QList<Device> current = mergeDevices(m_airPods->devices(), m_upower->devices());

    const QList<Device> vanished = vanishedDevices(devices(), current);
    if (vanished.isEmpty()) {
        m_graceTimer.stop();
        setDevices(current);
        return;
    }
    // Keep them for a moment; syncNow() drops them for good unless they came back by then.
    if (!m_graceTimer.isActive()) {
        m_graceTimer.start();
    }
    setDevices(mergeDevices(current, vanished));
}

void DeviceModel::syncNow()
{
    m_graceTimer.stop();
    setDevices(m_upower && m_airPods ? mergeDevices(m_airPods->devices(), m_upower->devices()) : QList<Device>{});
}
