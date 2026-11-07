// SPDX-License-Identifier: MIT
#include "aapprotocol.h"

#include <QtEndian>

namespace
{
constexpr char Header[] = {0x04, 0x00, 0x04, 0x00};
constexpr qsizetype OpcodeOffset = sizeof(Header);
constexpr quint16 BatteryOpcode = 0x0004;
constexpr quint16 EarDetectionOpcode = 0x0006;
constexpr quint16 ControlOpcode = 0x0009;

// Ear detection packet: header, opcode, then [primary] [secondary], one of EarState each.
constexpr qsizetype PrimaryOffset = OpcodeOffset + 2;
constexpr qsizetype SecondaryOffset = PrimaryOffset + 1;

enum EarState : quint8 {
    InEar = 0x00,
    OutOfEar = 0x01,
    InCase = 0x02,
};

// Control packet: header, opcode, [setting] [value] and three more bytes. Setting 0x0D is the
// noise control mode.
constexpr qsizetype SettingOffset = OpcodeOffset + 2;
constexpr qsizetype ValueOffset = SettingOffset + 1;
constexpr quint8 NoiseControlSetting = 0x0D;

enum NoiseMode : quint8 {
    ModeOff = 0x01,
    ModeCancellation = 0x02,
    ModeTransparency = 0x03,
    ModeAdaptive = 0x04,
};

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

Placement toPlacement(quint8 value)
{
    switch (value) {
    case InEar:
        return Placement::InEar;
    case OutOfEar:
        return Placement::OutOfEar;
    case InCase:
        return Placement::InCase;
    }
    return Placement::Unknown;
}

// Whether `packet` is a packet with this opcode and at least `size` bytes.
bool hasOpcode(QByteArrayView packet, quint16 opcode, qsizetype size)
{
    return packet.size() >= size && packet.startsWith(QByteArrayView(Header, sizeof(Header)))
        && qFromLittleEndian<quint16>(packet.data() + OpcodeOffset) == opcode;
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
    if (!hasOpcode(packet, BatteryOpcode, FirstEntryOffset)) {
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
        const int level = byteAt(entry, LevelField);
        parts.append({
            .component = *component,
            .percentage = level <= 100 ? level : -1, // anything else is not a level we can trust
            .charging = status == Charging,
        });
    }
    return parts;
}

std::optional<std::pair<Placement, Placement>> Aap::parseEarDetectionPacket(QByteArrayView packet)
{
    if (!hasOpcode(packet, EarDetectionOpcode, SecondaryOffset + 1)) {
        return std::nullopt;
    }
    return std::pair{toPlacement(byteAt(packet, PrimaryOffset)), toPlacement(byteAt(packet, SecondaryOffset))};
}

std::optional<NoiseControl> Aap::parseNoiseControlPacket(QByteArrayView packet)
{
    if (!hasOpcode(packet, ControlOpcode, ValueOffset + 1) || byteAt(packet, SettingOffset) != NoiseControlSetting) {
        return std::nullopt;
    }
    switch (byteAt(packet, ValueOffset)) {
    case ModeOff:
        return NoiseControl::Off;
    case ModeCancellation:
        return NoiseControl::Cancellation;
    case ModeTransparency:
        return NoiseControl::Transparency;
    case ModeAdaptive:
        return NoiseControl::Adaptive;
    }
    return NoiseControl::Unknown;
}
