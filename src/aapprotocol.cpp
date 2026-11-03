// SPDX-License-Identifier: MIT
#include "aapprotocol.h"

#include <QtEndian>

namespace
{
constexpr char Header[] = {0x04, 0x00, 0x04, 0x00};
constexpr qsizetype OpcodeOffset = sizeof(Header);
constexpr quint16 BatteryOpcode = 0x0004;

// Battery packet: header, opcode, entry count, then `count` entries of 5 bytes:
//   [component] [0x01] [level 0-100] [status] [0x01]
constexpr qsizetype CountOffset = OpcodeOffset + 2;
constexpr qsizetype FirstEntryOffset = CountOffset + 1;
constexpr qsizetype EntrySize = 5;
constexpr qsizetype ComponentField = 0;
constexpr qsizetype LevelField = 2;
constexpr qsizetype StatusField = 3;

enum Component : quint8 {
    Single = 0x01,
    Right = 0x02,
    Left = 0x04,
    Case = 0x08,
};

enum Status : quint8 {
    Unknown = 0x00,
    Charging = 0x01,
    Discharging = 0x02,
    Disconnected = 0x04,
};

std::optional<BatteryPart::Component> toComponent(quint8 value)
{
    switch (value) {
    case Single:
        return BatteryPart::Component::Main;
    case Right:
        return BatteryPart::Component::Right;
    case Left:
        return BatteryPart::Component::Left;
    case Case:
        return BatteryPart::Component::Case;
    }
    return std::nullopt;
}

quint8 byteAt(QByteArrayView data, qsizetype offset)
{
    return static_cast<quint8>(data[offset]);
}
}

QByteArray Aap::handshakePacket()
{
    return QByteArray::fromHex("00000400010002000000000000000000");
}

QByteArray Aap::requestNotificationsPacket()
{
    return QByteArray::fromHex("040004000f00ffffffff");
}

std::optional<QList<BatteryPart>> Aap::parseBatteryPacket(QByteArrayView packet)
{
    if (packet.size() < FirstEntryOffset || !packet.startsWith(QByteArrayView(Header, sizeof(Header)))) {
        return std::nullopt;
    }
    if (qFromLittleEndian<quint16>(packet.data() + OpcodeOffset) != BatteryOpcode) {
        return std::nullopt;
    }

    const qsizetype count = byteAt(packet, CountOffset);
    if (packet.size() < FirstEntryOffset + count * EntrySize) {
        return std::nullopt; // truncated
    }

    QList<BatteryPart> parts;
    for (qsizetype i = 0; i < count; ++i) {
        const QByteArrayView entry = packet.sliced(FirstEntryOffset + i * EntrySize, EntrySize);
        const auto component = toComponent(byteAt(entry, ComponentField));
        const quint8 status = byteAt(entry, StatusField);
        if (!component || status == Disconnected) {
            continue;
        }
        parts.append({
            .component = *component,
            .percentage = qMin(int(byteAt(entry, LevelField)), 100),
            .charging = status == Charging,
        });
    }
    return parts;
}
