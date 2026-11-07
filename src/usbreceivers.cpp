// SPDX-License-Identifier: MIT
#include "usbreceivers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>

namespace
{
const QString LogitechVendor = QStringLiteral("046d");
constexpr char HidInterfaceClass[] = "03";

QString readLine(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? QString::fromUtf8(file.readLine()).trimmed() : QString();
}

// Product names wireless receivers commonly have, unlike wired keyboards and mice.
bool looksLikeReceiver(const QString &product)
{
    static const QRegularExpression pattern(QStringLiteral("receiver|dongle|2\\.4 ?g|wireless|unifying|lightspeed|nano"),
                                            QRegularExpression::CaseInsensitiveOption);
    // "Wireless Charging Case": an AirPods case plugged in to charge, not a receiver.
    return pattern.match(product).hasMatch() && !product.contains(QLatin1String("charg"), Qt::CaseInsensitive);
}

bool hasHidInterface(const QDir &device)
{
    // Interfaces are the subdirectories named "<device>:<config>.<interface>".
    const QStringList interfaces = device.entryList({device.dirName() + QStringLiteral(":*")}, QDir::Dirs | QDir::NoDotAndDotDot);
    return std::ranges::any_of(interfaces, [&device](const QString &interface) {
        return readLine(device.filePath(interface + QStringLiteral("/bInterfaceClass"))) == QLatin1String(HidInterfaceClass);
    });
}

// The real sysfs paths of every power supply's device, e.g. the HID device of a mouse.
QStringList powerSupplyDevices(const QString &sysfs)
{
    QStringList paths;
    const QDir supplies(sysfs + QStringLiteral("/class/power_supply"));
    for (const QString &supply : supplies.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QString device = QFileInfo(supplies.filePath(supply + QStringLiteral("/device"))).canonicalFilePath();
        if (!device.isEmpty()) {
            paths.append(device);
        }
    }
    return paths;
}
}

QList<SilentDevice> usbReceiversWithoutBattery(const QString &sysfs)
{
    const QStringList supplies = powerSupplyDevices(sysfs);
    QList<SilentDevice> receivers;
    const QDir devices(sysfs + QStringLiteral("/bus/usb/devices"));
    for (const QString &name : devices.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
        const QDir device(devices.filePath(name));
        const QString vendor = readLine(device.filePath(QStringLiteral("idVendor")));
        const QString product = readLine(device.filePath(QStringLiteral("product")));
        if (vendor.isEmpty() || vendor == LogitechVendor || !looksLikeReceiver(product) || !hasHidInterface(device)) {
            continue; // interfaces and hubs have no idVendor; wired devices no receiver name
        }
        const QString path = QFileInfo(device.path()).canonicalFilePath() + QLatin1Char('/');
        const bool hasBattery = std::ranges::any_of(supplies, [&path](const QString &supply) {
            return supply.startsWith(path);
        });
        if (!hasBattery) {
            receivers.append({
                .key = QStringLiteral("usb:%1:%2").arg(vendor, readLine(device.filePath(QStringLiteral("idProduct")))),
                .name = product,
                .iconName = QStringLiteral("drive-removable-media-usb"),
                .audio = false,
                .usb = true,
            });
        }
    }
    return receivers;
}
