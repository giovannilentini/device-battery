// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QVariantMap>

// Turns the properties of an org.freedesktop.UPower.Device into a Device.
// Pure functions without D-Bus access, so they can be unit tested.
namespace UPower
{
QString deviceId(const QString &objectPath);

// False for laptop batteries, UPSes and AC adapters, which power the computer itself.
bool isPeripheral(const QVariantMap &properties);

// False while e.g. a receiver stays plugged in but its mouse is switched off.
bool isPresent(const QVariantMap &properties);

Device toDevice(const QString &objectPath, const QVariantMap &properties);
}
