// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QByteArray>
#include <QByteArrayView>

#include <optional>

// Apple Accessory Protocol (AAP): the private protocol AirPods and Beats speak with
// Apple hosts over an L2CAP channel. BlueZ does not implement it, which is why Linux
// normally shows no battery for AirPods. We only use the small read-only subset that
// reports battery levels; nothing here changes settings on the accessory.
//
// After the handshake every packet starts with the header 04 00 04 00, followed by a
// little-endian 16-bit opcode and an opcode-specific payload.
namespace Aap
{
// L2CAP Protocol/Service Multiplexer the accessory listens on.
constexpr quint16 Psm = 0x1001;

// Must be sent first, otherwise the accessory ignores every other packet.
QByteArray handshakePacket();

// Asks the accessory to push battery updates whenever a level or charging state changes.
QByteArray requestNotificationsPacket();

// Returns the components listed in a battery packet, or std::nullopt if `packet` is
// any other kind of packet (AirPods send many we do not care about).
// Components the accessory reports as unreachable (e.g. a closed case) are left out.
std::optional<QList<BatteryPart>> parseBatteryPacket(QByteArrayView packet);
}
