// SPDX-License-Identifier: MIT
//
// Runs AirPodsSource against a fake BlueZ on a private session bus (ctest starts the test
// under dbus-run-session, so it never touches the real system or session bus).
//
// What the AirPods themselves send is covered by aapsessiontest; here an Apple device only
// gets a session, which has not connected by the time the test ends.
#include "airpodssource.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QMutex>
#include <QSignalSpy>
#include <QTest>

using InterfaceMap = QMap<QString, QVariantMap>;
using ManagedObjects = QMap<QDBusObjectPath, InterfaceMap>;
Q_DECLARE_METATYPE(InterfaceMap)
Q_DECLARE_METATYPE(ManagedObjects)

namespace
{
const QString Service = QStringLiteral("org.bluez");
const QString Root = QStringLiteral("/");
const QString DeviceInterface = QStringLiteral("org.bluez.Device1");
const QString ObjectManagerInterface = QStringLiteral("org.freedesktop.DBus.ObjectManager");
const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString KeyboardPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_00_00_01");
const QString HeadsetPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_00_00_02");
const QString AirPodsPath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_00_00_03");
const QString AudioSinkUuid = QStringLiteral("0000110b-0000-1000-8000-00805f9b34fb");

QVariantMap device(const QString &address, const QString &name, const QString &icon, bool connected = true)
{
    return {
        {QStringLiteral("Address"), address},
        {QStringLiteral("Alias"), name},
        {QStringLiteral("Name"), name},
        {QStringLiteral("Icon"), icon},
        {QStringLiteral("Connected"), connected},
        {QStringLiteral("RSSI"), QVariant::fromValue<qint16>(-60)},
    };
}

QVariantMap airPods(bool connected = true)
{
    QVariantMap properties = device(QStringLiteral("aa:bb:cc:00:00:03"), QStringLiteral("My AirPods"), QStringLiteral("audio-headphones"), connected);
    properties.insert(QStringLiteral("Modalias"), QStringLiteral("bluetooth:v004Cp2014d0B5A"));
    properties.insert(QStringLiteral("UUIDs"), QStringList{AudioSinkUuid});
    return properties;
}

// Answers GetManagedObjects on its own bus connection, like bluetoothd would.
class FakeBlueZ : public QDBusVirtualObject
{
public:
    FakeBlueZ()
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-bluez")))
    {
        m_bus.registerVirtualObject(Root, this, QDBusConnection::SubPath);
    }

    ~FakeBlueZ() override
    {
        m_bus.unregisterObject(Root, QDBusConnection::UnregisterTree);
        QDBusConnection::disconnectFromBus(QStringLiteral("fake-bluez"));
    }

    bool start()
    {
        return m_bus.registerService(Service);
    }

    bool stop()
    {
        return m_bus.unregisterService(Service);
    }

    void setDevice(const QString &path, const QVariantMap &properties)
    {
        QMutexLocker lock(&m_mutex);
        m_devices.insert(path, properties);
    }

    void addDevice(const QString &path, const QVariantMap &properties)
    {
        setDevice(path, properties);
        auto signal = QDBusMessage::createSignal(Root, ObjectManagerInterface, QStringLiteral("InterfacesAdded"));
        signal << QVariant::fromValue(QDBusObjectPath(path)) << QVariant::fromValue(InterfaceMap{{DeviceInterface, properties}});
        m_bus.send(signal);
    }

    void removeDevice(const QString &path)
    {
        {
            QMutexLocker lock(&m_mutex);
            m_devices.remove(path);
        }
        auto signal = QDBusMessage::createSignal(Root, ObjectManagerInterface, QStringLiteral("InterfacesRemoved"));
        signal << QVariant::fromValue(QDBusObjectPath(path)) << QStringList{DeviceInterface};
        m_bus.send(signal);
    }

    void emitChanged(const QString &path, const QVariantMap &changes)
    {
        {
            QMutexLocker lock(&m_mutex);
            m_devices[path].insert(changes);
        }
        auto signal = QDBusMessage::createSignal(path, PropertiesInterface, QStringLiteral("PropertiesChanged"));
        signal << DeviceInterface << changes << QStringList();
        m_bus.send(signal);
    }

    QString introspect(const QString &) const override
    {
        return {};
    }

    bool handleMessage(const QDBusMessage &message, const QDBusConnection &connection) override
    {
        if (message.path() != Root || message.member() != QLatin1String("GetManagedObjects")) {
            return false;
        }
        ManagedObjects objects;
        {
            QMutexLocker lock(&m_mutex);
            for (auto it = m_devices.cbegin(); it != m_devices.cend(); ++it) {
                objects.insert(QDBusObjectPath(it.key()), {{DeviceInterface, it.value()}});
            }
        }
        connection.send(message.createReply(QVariant::fromValue(objects)));
        return true;
    }

private:
    QDBusConnection m_bus;
    QMutex m_mutex; // handleMessage() may run in QtDBus's thread
    QHash<QString, QVariantMap> m_devices;
};

QStringList names(const AirPodsSource &source)
{
    QStringList result;
    for (const SilentDevice &device : source.silentDevices()) {
        result << device.name;
    }
    result.sort();
    return result;
}
}

class AirPodsSourceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Refuse to run on a real bus: the fake claims org.bluez.
        QVERIFY2(qEnvironmentVariableIsSet("DEVICEBATTERY_PRIVATE_BUS"), "run through ctest, which starts a private bus");
        QVERIFY(QDBusConnection::sessionBus().isConnected());
        qDBusRegisterMetaType<InterfaceMap>();
        qDBusRegisterMetaType<ManagedObjects>();
    }

    void init()
    {
        m_fake = std::make_unique<FakeBlueZ>();
        m_fake->setDevice(KeyboardPath, device(QStringLiteral("aa:bb:cc:00:00:01"), QStringLiteral("Keyboard"), QStringLiteral("input-keyboard")));
        m_fake->setDevice(HeadsetPath, device(QStringLiteral("AA:BB:CC:00:00:02"), QStringLiteral("Headset"), QStringLiteral("audio-headset"), false));
        QVERIFY(m_fake->start());
    }

    void cleanup()
    {
        m_fake.reset();
    }

    void listsConnectedDevices()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")}); // the headset is not connected

        const SilentDevice keyboard = source.silentDevices().first();
        QCOMPARE(keyboard.key, QStringLiteral("bt:AA:BB:CC:00:00:01")); // as deviceKey() of a Bluetooth device
        QCOMPARE(keyboard.iconName, QStringLiteral("input-keyboard"));
        QVERIFY(!keyboard.audio);
        QVERIFY(!keyboard.usb);
        QVERIFY(source.devices().isEmpty()); // no battery level comes from BlueZ
    }

    void followsConnectionsAndNames()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        m_fake->emitChanged(HeadsetPath, {{QStringLiteral("Connected"), true}});
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Headset"), QStringLiteral("Keyboard")}));
        QVERIFY(std::ranges::any_of(source.silentDevices(), &SilentDevice::audio));

        m_fake->emitChanged(KeyboardPath, {{QStringLiteral("Alias"), QStringLiteral("Desk keyboard")}});
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Desk keyboard"), QStringLiteral("Headset")}));

        m_fake->emitChanged(KeyboardPath, {{QStringLiteral("Connected"), false}});
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Headset")});
    }

    void followsAddedAndRemovedDevices()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        const QString mousePath = QStringLiteral("/org/bluez/hci0/dev_AA_BB_CC_00_00_04");
        m_fake->addDevice(mousePath, device(QStringLiteral("AA:BB:CC:00:00:04"), QStringLiteral("Mouse"), QStringLiteral("input-mouse")));
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Keyboard"), QStringLiteral("Mouse")}));

        m_fake->removeDevice(mousePath);
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});
    }

    // Properties the widget does not use change often (signal strength while something scans):
    // they must not make it do anything.
    void ignoresIrrelevantChanges()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        QSignalSpy changes(&source, &AirPodsSource::connectionsChanged);
        m_fake->emitChanged(KeyboardPath, {{QStringLiteral("RSSI"), QVariant::fromValue<qint16>(-70)}});
        // A relevant change sent after it marks the point by which the first one was handled.
        m_fake->emitChanged(KeyboardPath, {{QStringLiteral("Alias"), QStringLiteral("Desk keyboard")}});
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Desk keyboard")});
        QCOMPARE(changes.size(), 1);
    }

    // AirPods get an AAP session instead of a place among the devices without a battery; until
    // it delivers a report, they are not listed at all.
    void tracksAirPodsSeparately()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        m_fake->addDevice(AirPodsPath, airPods(false));
        // Not connected: like any other device, and so not listed.
        m_fake->emitChanged(HeadsetPath, {{QStringLiteral("Connected"), true}});
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Headset"), QStringLiteral("Keyboard")}));

        m_fake->emitChanged(AirPodsPath, {{QStringLiteral("Connected"), true}});
        m_fake->emitChanged(HeadsetPath, {{QStringLiteral("Connected"), false}});
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});
        QVERIFY(source.devices().isEmpty());
    }

    // An Apple keyboard has the vendor ID but no audio profile: UPower reports its battery.
    void appleDevicesWithoutAudioAreOrdinary()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        QVariantMap magicKeyboard = airPods();
        magicKeyboard.insert(QStringLiteral("Alias"), QStringLiteral("Magic Keyboard"));
        magicKeyboard.remove(QStringLiteral("UUIDs"));
        m_fake->addDevice(AirPodsPath, magicKeyboard);
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Keyboard"), QStringLiteral("Magic Keyboard")}));
    }

    // bluetoothd sends no InterfacesRemoved when it exits; its devices must go, and come back
    // when it starts again.
    void recoversWhenBlueZRestarts()
    {
        AirPodsSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});

        QVERIFY(m_fake->stop());
        QTRY_VERIFY(source.silentDevices().isEmpty());

        m_fake->setDevice(HeadsetPath, device(QStringLiteral("AA:BB:CC:00:00:02"), QStringLiteral("Headset"), QStringLiteral("audio-headset")));
        QVERIFY(m_fake->start());
        QTRY_COMPARE(names(source), (QStringList{QStringLiteral("Headset"), QStringLiteral("Keyboard")}));

        // Still subscribed after the restart.
        m_fake->emitChanged(HeadsetPath, {{QStringLiteral("Connected"), false}});
        QTRY_COMPARE(names(source), QStringList{QStringLiteral("Keyboard")});
    }

private:
    std::unique_ptr<FakeBlueZ> m_fake;
};

QTEST_GUILESS_MAIN(AirPodsSourceTest)
#include "airpodssourcetest.moc"
