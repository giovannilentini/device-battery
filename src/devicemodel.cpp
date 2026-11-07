// SPDX-License-Identifier: MIT
#include "devicemodel.h"

#include "logging.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusPendingCall>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QStandardPaths>

#include <algorithm>
#include <chrono>
#include <iterator>

namespace
{
// How long a device that disappeared stays on screen, so that one that comes right back
// (e.g. a Logitech mouse waking up, which recreates its UPower object) does not blink.
constexpr auto GracePeriod = std::chrono::seconds(2);

QString recentDevicesFile()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/devicebattery/recent-devices.json");
}
}

DeviceModel::DeviceModel(QObject *parent)
    : DeviceListModel(parent)
{
    m_graceTimer.setSingleShot(true);
    m_graceTimer.setInterval(GracePeriod);
    connect(&m_graceTimer, &QTimer::timeout, this, &DeviceModel::syncNow);
    loadRecent();
    connect(this, &DeviceListModel::summaryChanged, this, &DeviceModel::notifyLowBatteries);
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
    setDevices(withRecent({}));
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
    m_graceTimer.stop();
    setDevices({}); // not "disconnected": nothing watches them any more
}

void DeviceModel::sync()
{
    // AirPods data is richer (per earbud and case), so it wins over a UPower entry
    // for the same Bluetooth address, e.g. when PipeWire also reports a headset level.
    const QList<Device> current = mergeDevices(m_airPods->devices(), m_upower->devices());

    const QList<Device> vanished = vanishedDevices(connectedDevices(), current);
    if (vanished.isEmpty()) {
        m_graceTimer.stop();
        setDevices(withRecent(current));
        return;
    }
    // Keep them for a moment; syncNow() drops them for good unless they came back by then.
    if (!m_graceTimer.isActive()) {
        m_graceTimer.start();
    }
    setDevices(withRecent(mergeDevices(current, vanished)));
}

void DeviceModel::syncNow()
{
    m_graceTimer.stop();
    if (!m_upower || !m_airPods) {
        return;
    }
    const QList<Device> current = mergeDevices(m_airPods->devices(), m_upower->devices());
    const QList<Device> gone = vanishedDevices(connectedDevices(), current);
    const QList<Device> recent = updateRecentDevices(m_recent, gone, current, QDateTime::currentMSecsSinceEpoch());
    if (recent != m_recent) {
        m_recent = recent;
        saveRecent();
    }
    setDevices(withRecent(current));
}

QList<Device> DeviceModel::connectedDevices() const
{
    QList<Device> connected;
    std::ranges::copy_if(devices(), std::back_inserter(connected), &Device::connected);
    return connected;
}

QList<Device> DeviceModel::withRecent(const QList<Device> &connected) const
{
    // Connected devices first, then the disconnected ones that did not come back.
    QList<Device> result = connected;
    for (const Device &device : m_recent) {
        if (std::ranges::none_of(connected, [&device](const Device &other) {
                return isSameDevice(device, other);
            })) {
            result.append(device);
        }
    }
    return result;
}

void DeviceModel::loadRecent()
{
    QFile file(recentDevicesFile());
    if (file.open(QIODevice::ReadOnly)) {
        m_recent = recentDevicesFromJson(file.readAll());
    }
}

void DeviceModel::saveRecent() const
{
    const QString path = recentDevicesFile();
    QDir().mkpath(QFileInfo(path).path());
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(recentDevicesToJson(m_recent)) < 0 || !file.commit()) {
        qCWarning(DEVICEBATTERY) << "Cannot save the recently disconnected devices to" << path << ":" << file.errorString();
    }
}

void DeviceModel::configureNotifications(bool enabled, int threshold, const QString &title, const QString &body)
{
    m_notify = enabled;
    m_notifyThreshold = threshold;
    m_notifyTitle = title;
    m_notifyBody = body;
    notifyLowBatteries();
}

void DeviceModel::notifyLowBatteries()
{
    QList<Device> airPods;
    std::ranges::copy_if(devices(), std::back_inserter(airPods), [](const Device &device) {
        return device.id.startsWith(QLatin1String("aap:"));
    });
    // Runs even while disabled, so turning notifications on does not warn about a battery
    // that was already low.
    const QList<Device> toWarn = devicesToWarn(airPods, m_warned, m_notifyThreshold);
    if (!m_notify || m_notifyTitle.isEmpty()) {
        return;
    }
    for (const Device &device : toWarn) {
        qCDebug(DEVICEBATTERY) << "Low battery notification for" << device.name << device.percentage;
        QDBusMessage notify = QDBusMessage::createMethodCall(QStringLiteral("org.freedesktop.Notifications"),
                                                             QStringLiteral("/org/freedesktop/Notifications"),
                                                             QStringLiteral("org.freedesktop.Notifications"),
                                                             QStringLiteral("Notify"));
        notify << QStringLiteral("Device Battery") // app_name
               << 0u // replaces_id
               << QStringLiteral("battery-caution") // app_icon
               << m_notifyTitle.arg(device.name) // summary
               << m_notifyBody.arg(device.percentage) // body
               << QStringList() // actions
               << QVariantMap{{QStringLiteral("urgency"), QVariant::fromValue<uchar>(1)}} // normal
               << -1; // default timeout
        QDBusConnection::sessionBus().asyncCall(notify);
    }
}
