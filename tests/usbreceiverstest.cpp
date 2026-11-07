// SPDX-License-Identifier: MIT
#include "usbreceivers.h"

#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTest>

// Builds a small fake /sys: real directories under devices/, reached through the symlinks
// bus/usb/devices/* and class/power_supply/*/device, like the kernel's.
class UsbReceiversTest : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir m_sysfs;

    void write(const QString &path, const QByteArray &content)
    {
        QDir().mkpath(QFileInfo(m_sysfs.filePath(path)).path());
        QFile file(m_sysfs.filePath(path));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(content + '\n');
    }

    // A USB device at devices/usb7/<port> with one interface of the given class.
    void addUsbDevice(const QString &port, const QByteArray &vendor, const QByteArray &product, const QByteArray &name, const QByteArray &interfaceClass = "03")
    {
        const QString device = QStringLiteral("devices/usb7/") + port;
        write(device + QStringLiteral("/idVendor"), vendor);
        write(device + QStringLiteral("/idProduct"), product);
        write(device + QStringLiteral("/product"), name);
        write(device + QLatin1Char('/') + port + QStringLiteral(":1.0/bInterfaceClass"), interfaceClass);
        QDir().mkpath(m_sysfs.filePath(QStringLiteral("bus/usb/devices")));
        QVERIFY(QFile::link(m_sysfs.filePath(device), m_sysfs.filePath(QStringLiteral("bus/usb/devices/") + port)));
    }

    // A power supply whose device lives inside the USB device at `port`.
    void addBattery(const QString &supply, const QString &port)
    {
        const QString hid = QStringLiteral("devices/usb7/%1/%1:1.2/0003:1234:5678.0001").arg(port);
        QDir().mkpath(m_sysfs.filePath(hid));
        QDir().mkpath(m_sysfs.filePath(QStringLiteral("class/power_supply/") + supply));
        QVERIFY(QFile::link(m_sysfs.filePath(hid), m_sysfs.filePath(QStringLiteral("class/power_supply/%1/device").arg(supply))));
    }

private Q_SLOTS:
    void init()
    {
        QVERIFY(m_sysfs.isValid());
        QDir(m_sysfs.path()).removeRecursively();
        QDir().mkpath(m_sysfs.path());
    }

    void findsReceiverWithoutBattery()
    {
        addUsbDevice(QStringLiteral("7-1.4"), "05ac", "024f", "2.4G Dongle");

        const QList<SilentDevice> receivers = usbReceiversWithoutBattery(m_sysfs.path());
        QCOMPARE(receivers.size(), 1);
        QCOMPARE(receivers.first().key, QStringLiteral("usb:05ac:024f"));
        QCOMPARE(receivers.first().name, QStringLiteral("2.4G Dongle"));
        QVERIFY(receivers.first().usb);
    }

    void skipsReceiverWithBattery()
    {
        addUsbDevice(QStringLiteral("7-1.4"), "1915", "1025", "Wireless Receiver");
        addBattery(QStringLiteral("hid-battery"), QStringLiteral("7-1.4"));
        QVERIFY(usbReceiversWithoutBattery(m_sysfs.path()).isEmpty());
    }

    // A battery of another device, at a port whose name starts the same, does not count.
    void batteryMustBeInsideTheReceiver()
    {
        addUsbDevice(QStringLiteral("7-1"), "1915", "1025", "Wireless Receiver");
        addUsbDevice(QStringLiteral("7-10"), "1234", "0001", "Other Dongle");
        addBattery(QStringLiteral("hid-battery"), QStringLiteral("7-10"));
        const QList<SilentDevice> receivers = usbReceiversWithoutBattery(m_sysfs.path());
        QCOMPARE(receivers.size(), 1);
        QCOMPARE(receivers.first().name, QStringLiteral("Wireless Receiver"));
    }

    void skipsWiredLogitechChargersAndNonHid()
    {
        addUsbDevice(QStringLiteral("7-1.1"), "04d9", "0006", "USB Keyboard"); // wired
        addUsbDevice(QStringLiteral("7-1.2"), "046d", "c539", "USB Receiver"); // Logitech: HID++
        addUsbDevice(QStringLiteral("7-1.3"), "0bda", "8922", "Wireless Adapter", "e0"); // a radio
        addUsbDevice(QStringLiteral("7-1.5"), "05ac", "13a0", "Wireless Charging Case"); // AirPods case
        QVERIFY(usbReceiversWithoutBattery(m_sysfs.path()).isEmpty());
    }

    void missingSysfsIsEmpty()
    {
        QVERIFY(usbReceiversWithoutBattery(m_sysfs.filePath(QStringLiteral("nothing"))).isEmpty());
    }
};

QTEST_GUILESS_MAIN(UsbReceiversTest)
#include "usbreceiverstest.moc"
