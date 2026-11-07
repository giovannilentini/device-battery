// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

// USB wireless receivers ("2.4G Dongle", "Wireless Receiver", ...) for which the kernel has no
// battery, read from sysfs. Their keyboard or mouse may well have a battery, but it reports it
// in a way only the vendor's app understands, so Linux cannot show it.
//
// Logitech receivers are left out: the kernel reads their devices' batteries (HID++) whenever
// the device is awake, so a missing battery there just means the mouse is asleep.
//
// Reads a few small sysfs files; meant to be called when the popup opens, not periodically.
// `sysfs` is "/sys" except in tests.
QList<SilentDevice> usbReceiversWithoutBattery(const QString &sysfs = QStringLiteral("/sys"));
