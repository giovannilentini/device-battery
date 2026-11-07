// SPDX-License-Identifier: MIT
#include "airpodssource.h"

#include "aapsession.h"
#include "dbusutils.h"

#include <QDBusObjectPath>
#include <QDBusServiceWatcher>

namespace
{
const QString Service = QStringLiteral("org.bluez");
const QString DeviceInterface = QStringLiteral("org.bluez.Device1");
const QString ObjectManagerInterface = QStringLiteral("org.freedesktop.DBus.ObjectManager");

// Apple's Bluetooth SIG company ID is 0x004C; AirPods and Beats both use it.
const QString AppleModaliasPrefix = QStringLiteral("bluetooth:v004C");
const QString AudioSinkUuid = QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb");
const QString HandsfreeUuid = QStringLiteral("0000111e-0000-1000-8000-00805f9b34fb");

// The Device1 properties isConnectedAppleAudio(), displayName() and start() read. Changes of
// any other property (e.g. RSSI while something scans) are ignored without further work.
const QStringList RelevantKeys{
    QStringLiteral("Address"),
    QStringLiteral("Alias"),
    QStringLiteral("Connected"),
    QStringLiteral("Modalias"),
    QStringLiteral("Name"),
    QStringLiteral("UUIDs"),
};

QVariantMap relevantPart(const QVariantMap &properties)
{
    QVariantMap result;
    for (const QString &key : RelevantKeys) {
        const auto it = properties.constFind(key);
        if (it != properties.constEnd()) {
            result.insert(key, *it);
        }
    }
    return result;
}

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;

// Apple keyboards, mice and trackpads share the vendor ID but report their battery through
// the kernel/UPower already, so we also require an audio profile.
bool isConnectedAppleAudio(const QVariantMap &properties)
{
    if (!properties.value(QStringLiteral("Connected")).toBool()) {
        return false;
    }
    if (!properties.value(QStringLiteral("Modalias")).toString().startsWith(AppleModaliasPrefix, Qt::CaseInsensitive)) {
        return false;
    }
    const QStringList uuids = properties.value(QStringLiteral("UUIDs")).toStringList();
    return uuids.contains(AudioSinkUuid, Qt::CaseInsensitive) || uuids.contains(HandsfreeUuid, Qt::CaseInsensitive);
}

QString displayName(const QVariantMap &properties)
{
    const QString alias = properties.value(QStringLiteral("Alias")).toString();
    return alias.isEmpty() ? properties.value(QStringLiteral("Name")).toString() : alias;
}

QString deviceId(const QString &address)
{
    return QStringLiteral("aap:") + address;
}
}

AirPodsSource::AirPodsSource(const QDBusConnection &bus, QObject *parent)
    : DeviceSource(parent)
    , m_bus(bus)
{
    const QString root = QStringLiteral("/");
    m_bus.connect(Service, root, ObjectManagerInterface, QStringLiteral("InterfacesAdded"), this, SLOT(onInterfacesAdded(QDBusMessage)));
    m_bus.connect(Service, root, ObjectManagerInterface, QStringLiteral("InterfacesRemoved"), this, SLOT(onInterfacesRemoved(QDBusMessage)));
    // A single match rule for every device; the bus filters out changes of other interfaces
    // (media transports, players, ...), but changes of every device's Device1 still arrive.
    m_bus.connect(Service,
                  QString(),
                  DBus::PropertiesInterface,
                  QStringLiteral("PropertiesChanged"),
                  {DeviceInterface},
                  QString(),
                  this,
                  SLOT(onPropertiesChanged(QDBusMessage)));

    // bluetoothd sends no InterfacesRemoved when it exits, so without this we would keep dead
    // sessions and stale levels after a restart. Also covers bluetoothd starting after us.
    auto *watcher = new QDBusServiceWatcher(Service, m_bus, QDBusServiceWatcher::WatchForOwnerChange, this);
    connect(watcher, &QDBusServiceWatcher::serviceOwnerChanged, this, [this](const QString &, const QString &, const QString &newOwner) {
        qCDebug(DEVICEBATTERY) << "BlueZ" << (newOwner.isEmpty() ? "went away" : "(re)started");
        reset();
        if (!newOwner.isEmpty()) {
            enumerate();
        }
    });

    enumerate();
}

void AirPodsSource::enumerate()
{
    const auto getObjects = QDBusMessage::createMethodCall(Service, QStringLiteral("/"), ObjectManagerInterface, QStringLiteral("GetManagedObjects"));
    DBus::callAsync(m_bus, getObjects, this, [this](const QDBusMessage &reply) {
        const auto objects = DBus::argument<ManagedObjects>(reply, 0);
        for (auto object = objects.cbegin(); object != objects.cend(); ++object) {
            if (object->contains(DeviceInterface)) {
                addDevice(object.key().path(), object->value(DeviceInterface));
            }
        }
    });
}

void AirPodsSource::reset()
{
    for (const Tracked &tracked : std::as_const(m_tracked)) {
        delete tracked.session;
    }
    m_tracked.clear();
    m_properties.clear();
    clear();
}

void AirPodsSource::onInterfacesAdded(const QDBusMessage &message)
{
    const auto interfaces = DBus::argument<InterfaceMap>(message, 1);
    if (interfaces.contains(DeviceInterface)) {
        addDevice(DBus::argument<QDBusObjectPath>(message, 0).path(), interfaces.value(DeviceInterface));
    }
}

void AirPodsSource::onInterfacesRemoved(const QDBusMessage &message)
{
    if (!DBus::argument<QStringList>(message, 1).contains(DeviceInterface)) {
        return;
    }
    const QString path = DBus::argument<QDBusObjectPath>(message, 0).path();
    m_properties.remove(path);
    stop(path);
}

void AirPodsSource::onPropertiesChanged(const QDBusMessage &message)
{
    const auto cached = m_properties.find(message.path());
    if (cached == m_properties.end()) {
        return; // InterfacesAdded for this device has not arrived yet
    }
    bool relevant = false;
    const QVariantMap changed = DBus::argument<QVariantMap>(message, 1);
    for (auto it = changed.cbegin(); it != changed.cend(); ++it) {
        if (RelevantKeys.contains(it.key())) {
            cached->insert(it.key(), it.value());
            relevant = true;
        }
    }
    for (const QString &invalidated : DBus::argument<QStringList>(message, 2)) {
        relevant |= cached->remove(invalidated) > 0;
    }
    if (relevant) {
        evaluate(message.path());
    }
}

void AirPodsSource::addDevice(const QString &path, const QVariantMap &properties)
{
    m_properties.insert(path, relevantPart(properties));
    evaluate(path);
}

void AirPodsSource::evaluate(const QString &path)
{
    const QVariantMap properties = m_properties.value(path);
    if (!isConnectedAppleAudio(properties)) {
        stop(path);
        return;
    }

    const auto tracked = m_tracked.find(path);
    if (tracked == m_tracked.end()) {
        start(path, properties);
        return;
    }
    // Already connected: the only thing that can change is the user-visible name.
    const QString name = displayName(properties);
    if (tracked->name != name) {
        tracked->name = name;
        publish(*tracked);
    }
}

void AirPodsSource::start(const QString &path, const QVariantMap &properties)
{
    Tracked tracked{
        .address = properties.value(QStringLiteral("Address")).toString().toUpper(),
        .name = displayName(properties),
        .session = nullptr,
        .parts = {},
        .model = appleModelName(properties.value(QStringLiteral("Modalias")).toString()),
    };
    if (tracked.name.contains(tracked.model, Qt::CaseInsensitive)) {
        tracked.model.clear(); // e.g. "AirPods Max" named "AirPods Max": nothing to add
    }
    qCDebug(DEVICEBATTERY) << "Tracking" << tracked.name << tracked.address;
    tracked.session = new AapSession(tracked.address, this);
    connect(tracked.session, &AapSession::batteryChanged, this, [this, path](const QList<BatteryPart> &parts) {
        const auto entry = m_tracked.find(path);
        if (entry != m_tracked.end()) {
            entry->parts = parts;
            publish(*entry);
        }
    });
    connect(tracked.session, &AapSession::placementsChanged, this, [this, path](Placement primary, Placement secondary) {
        const auto entry = m_tracked.find(path);
        if (entry != m_tracked.end()) {
            entry->primary = primary;
            entry->secondary = secondary;
            publish(*entry);
        }
    });
    connect(tracked.session, &AapSession::noiseControlChanged, this, [this, path](NoiseControl mode) {
        const auto entry = m_tracked.find(path);
        if (entry != m_tracked.end()) {
            entry->noiseControl = mode;
            publish(*entry);
        }
    });
    connect(tracked.session, &AapSession::batteryLost, this, [this, path] {
        const auto entry = m_tracked.find(path);
        if (entry != m_tracked.end()) {
            entry->parts.clear();
            entry->primary = entry->secondary = Placement::Unknown;
            entry->noiseControl = NoiseControl::Unknown;
            publish(*entry); // withdraws the device; a UPower entry for it may show instead
        }
    });
    m_tracked.insert(path, tracked);
}

void AirPodsSource::stop(const QString &path)
{
    const auto tracked = m_tracked.find(path);
    if (tracked == m_tracked.end()) {
        return;
    }
    qCDebug(DEVICEBATTERY) << "Stopped tracking" << tracked->name << tracked->address;
    const QString id = deviceId(tracked->address);
    delete tracked->session;
    m_tracked.erase(tracked);
    remove(id);
}

void AirPodsSource::publish(const Tracked &tracked)
{
    if (tracked.parts.isEmpty()) {
        remove(deviceId(tracked.address)); // every component is docked or out of range
        return;
    }
    Device device;
    device.id = deviceId(tracked.address);
    device.address = tracked.address;
    device.name = tracked.name;
    device.model = tracked.model;
    device.noiseControl = tracked.noiseControl;
    QList<BatteryPart> parts = tracked.parts;
    applyPlacements(parts, tracked.primary, tracked.secondary);
    applyBatteryParts(device, parts); // fills kind, levels and parts
    upsert(device);
}
