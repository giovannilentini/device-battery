// SPDX-License-Identifier: MIT
//
// Runs UPowerSource against a fake UPower on a private session bus (ctest starts the test
// under dbus-run-session, so it never touches the real system or session bus).
#include "upowersource.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusMetaType>
#include <QDBusObjectPath>
#include <QDBusVirtualObject>
#include <QMutex>
#include <QTest>

namespace
{
const QString Service = QStringLiteral("org.freedesktop.UPower");
const QString ManagerPath = QStringLiteral("/org/freedesktop/UPower");
const QString DeviceInterface = QStringLiteral("org.freedesktop.UPower.Device");
const QString PropertiesInterface = QStringLiteral("org.freedesktop.DBus.Properties");
const QString MousePath = QStringLiteral("/org/freedesktop/UPower/devices/mouse_hidpp_battery_0");
const QString BatteryPath = QStringLiteral("/org/freedesktop/UPower/devices/battery_BAT0");

constexpr uint TypeMouse = 5;
constexpr uint StateUnknown = 0;
constexpr uint StateDischarging = 2;

QVariantMap mouse(double percentage, uint state = StateDischarging)
{
    return {
        {QStringLiteral("Model"), QStringLiteral("M720")},
        {QStringLiteral("Type"), TypeMouse},
        {QStringLiteral("PowerSupply"), false},
        {QStringLiteral("IsPresent"), true},
        {QStringLiteral("Percentage"), percentage},
        {QStringLiteral("State"), state},
    };
}

// Answers EnumerateDevices and GetAll on its own bus connection, like upowerd would.
class FakeUPower : public QDBusVirtualObject
{
public:
    FakeUPower()
        : m_bus(QDBusConnection::connectToBus(QDBusConnection::SessionBus, QStringLiteral("fake-upower")))
    {
        m_bus.registerVirtualObject(ManagerPath, this, QDBusConnection::SubPath);
    }

    ~FakeUPower() override
    {
        m_bus.unregisterObject(ManagerPath, QDBusConnection::UnregisterTree);
        QDBusConnection::disconnectFromBus(QStringLiteral("fake-upower"));
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

    void removeDevice(const QString &path)
    {
        QMutexLocker lock(&m_mutex);
        m_devices.remove(path);
    }

    // Emitted right after the next GetAll reply for `path`: the "UPower fills in the real
    // level right after announcing a device" case.
    void changeAfterNextRead(const QString &path, const QVariantMap &changes)
    {
        QMutexLocker lock(&m_mutex);
        m_changeAfterRead.insert(path, changes);
    }

    void emitDeviceAdded(const QString &path)
    {
        auto signal = QDBusMessage::createSignal(ManagerPath, Service, QStringLiteral("DeviceAdded"));
        signal << QVariant::fromValue(QDBusObjectPath(path));
        m_bus.send(signal);
    }

    void emitDeviceRemoved(const QString &path)
    {
        auto signal = QDBusMessage::createSignal(ManagerPath, Service, QStringLiteral("DeviceRemoved"));
        signal << QVariant::fromValue(QDBusObjectPath(path));
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
        if (message.path() == ManagerPath && message.member() == QLatin1String("EnumerateDevices")) {
            QMutexLocker lock(&m_mutex);
            QList<QDBusObjectPath> paths;
            for (const QString &path : m_devices.keys()) {
                paths << QDBusObjectPath(path);
            }
            connection.send(message.createReply(QVariant::fromValue(paths)));
            return true;
        }
        if (message.interface() == PropertiesInterface && message.member() == QLatin1String("GetAll")) {
            QVariantMap properties;
            std::optional<QVariantMap> changeAfter;
            {
                QMutexLocker lock(&m_mutex);
                const auto it = m_devices.constFind(message.path());
                if (it == m_devices.constEnd()) {
                    connection.send(message.createErrorReply(QStringLiteral("org.freedesktop.DBus.Error.UnknownObject"), message.path()));
                    return true;
                }
                properties = *it;
                if (m_changeAfterRead.contains(message.path())) {
                    changeAfter = m_changeAfterRead.take(message.path());
                    m_devices[message.path()].insert(*changeAfter);
                }
            }
            connection.send(message.createReply(QVariant::fromValue(properties)));
            if (changeAfter) {
                auto signal = QDBusMessage::createSignal(message.path(), PropertiesInterface, QStringLiteral("PropertiesChanged"));
                signal << DeviceInterface << *changeAfter << QStringList();
                connection.send(signal);
            }
            return true;
        }
        return false;
    }

private:
    QDBusConnection m_bus;
    QMutex m_mutex; // handleMessage() may run in QtDBus's thread
    QHash<QString, QVariantMap> m_devices;
    QHash<QString, QVariantMap> m_changeAfterRead;
};

int percentageOf(const UPowerSource &source, const QString &path)
{
    for (const Device &device : source.devices()) {
        if (device.id == QStringLiteral("upower:") + path) {
            return device.percentage;
        }
    }
    return -2; // not shown
}
}

class UPowerSourceTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        // Refuse to run on a real bus: the fake claims org.freedesktop.UPower.
        QVERIFY2(qEnvironmentVariableIsSet("DEVICEBATTERY_PRIVATE_BUS"), "run through ctest, which starts a private bus");
        QVERIFY(QDBusConnection::sessionBus().isConnected());
    }

    void init()
    {
        m_fake = std::make_unique<FakeUPower>();
        m_fake->setDevice(MousePath, mouse(54));
        m_fake->setDevice(BatteryPath, {{QStringLiteral("PowerSupply"), true}, {QStringLiteral("Percentage"), 80.0}});
        QVERIFY(m_fake->start());
    }

    void cleanup()
    {
        m_fake.reset();
    }

    void showsPeripheralsOnly()
    {
        UPowerSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(source.devices().size(), 1);
        QCOMPARE(source.devices().at(0).name, QStringLiteral("M720"));
        QCOMPARE(source.devices().at(0).percentage, 54);
    }

    void followsLevelChanges()
    {
        UPowerSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(percentageOf(source, MousePath), 54);

        m_fake->emitChanged(MousePath, {{QStringLiteral("Percentage"), 41.0}});
        QTRY_COMPARE(percentageOf(source, MousePath), 41);
    }

    void followsAddedAndRemovedDevices()
    {
        UPowerSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(source.devices().size(), 1);

        const QString keyboard = QStringLiteral("/org/freedesktop/UPower/devices/keyboard_0");
        m_fake->setDevice(keyboard, mouse(90));
        m_fake->emitDeviceAdded(keyboard);
        QTRY_COMPARE(source.devices().size(), 2);

        m_fake->removeDevice(keyboard);
        m_fake->emitDeviceRemoved(keyboard);
        QTRY_COMPARE(source.devices().size(), 1);
    }

    // UPower announces a device with a placeholder and fills in the level right after: that
    // update must not be lost between reading the properties and subscribing to changes.
    void keepsLevelSentRightAfterAnnouncement()
    {
        UPowerSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(source.devices().size(), 1);

        const QString receiver = QStringLiteral("/org/freedesktop/UPower/devices/mouse_hidpp_battery_1");
        m_fake->setDevice(receiver, mouse(0, StateUnknown));
        m_fake->changeAfterNextRead(receiver, {{QStringLiteral("Percentage"), 70.0}, {QStringLiteral("State"), StateDischarging}});
        m_fake->emitDeviceAdded(receiver);
        QTRY_COMPARE(percentageOf(source, receiver), 70);
    }

    // upowerd sends no DeviceRemoved when it exits; its devices must go, and come back
    // when it starts again.
    void recoversWhenUPowerRestarts()
    {
        UPowerSource source(QDBusConnection::sessionBus());
        QTRY_COMPARE(source.devices().size(), 1);

        QVERIFY(m_fake->stop());
        QTRY_VERIFY(source.devices().isEmpty());

        m_fake->setDevice(MousePath, mouse(33));
        QVERIFY(m_fake->start());
        QTRY_COMPARE(percentageOf(source, MousePath), 33);

        // Subscriptions were renewed: changes keep arriving after the restart.
        m_fake->emitChanged(MousePath, {{QStringLiteral("Percentage"), 32.0}});
        QTRY_COMPARE(percentageOf(source, MousePath), 32);
    }

private:
    std::unique_ptr<FakeUPower> m_fake;
};

QTEST_GUILESS_MAIN(UPowerSourceTest)
#include "upowersourcetest.moc"
