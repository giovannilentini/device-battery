// SPDX-License-Identifier: MIT
#include "devicelistmodel.h"

#include "logging.h"

#include <QSet>

#include <algorithm>

namespace
{
QVariantList partsToVariant(const QList<BatteryPart> &parts)
{
    QVariantList list;
    list.reserve(parts.size());
    for (const BatteryPart &part : parts) {
        list.append(QVariantMap{
            {QStringLiteral("component"), componentKey(part.component)},
            {QStringLiteral("percentage"), part.percentage},
            {QStringLiteral("charging"), part.charging},
        });
    }
    return list;
}

// The roles whose value differs between two versions of the same device. Views only
// re-evaluate bindings of these roles; e.g. a level update leaves the name and icon alone.
QList<int> changedRoles(const Device &before, const Device &after)
{
    QList<int> roles;
    if (before.name != after.name) {
        roles << Qt::DisplayRole << DeviceListModel::NameRole;
    }
    if (before.kind != after.kind) {
        roles << DeviceListModel::IconNameRole;
    }
    if (before.percentage != after.percentage) {
        roles << DeviceListModel::PercentageRole;
    }
    if (before.charging != after.charging) {
        roles << DeviceListModel::ChargingRole;
    }
    if (before.parts != after.parts) {
        roles << DeviceListModel::PartsRole;
    }
    return roles;
}
}

DeviceListModel::DeviceListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int DeviceListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_devices.size();
}

int DeviceListModel::count() const
{
    return m_devices.size();
}

QVariant DeviceListModel::data(const QModelIndex &index, int role) const
{
    if (!checkIndex(index, CheckIndexOption::IndexIsValid | CheckIndexOption::ParentIsInvalid)) {
        return {};
    }
    const Device &device = m_devices.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
    case NameRole:
        return device.name;
    case IconNameRole:
        return iconName(device.kind);
    case PercentageRole:
        return device.percentage;
    case ChargingRole:
        return device.charging;
    case PartsRole:
        return partsToVariant(device.parts);
    }
    return {};
}

QHash<int, QByteArray> DeviceListModel::roleNames() const
{
    return {
        {NameRole, "name"},
        {IconNameRole, "iconName"},
        {PercentageRole, "percentage"},
        {ChargingRole, "charging"},
        {PartsRole, "parts"},
    };
}

QVariantList DeviceListModel::summary() const
{
    QVariantList list;
    list.reserve(m_devices.size());
    for (const Device &device : m_devices) {
        list.append(QVariantMap{
            {QStringLiteral("name"), device.name},
            {QStringLiteral("iconName"), iconName(device.kind)},
            {QStringLiteral("percentage"), device.percentage},
            {QStringLiteral("charging"), device.charging},
        });
    }
    return list;
}

int DeviceListModel::rowOf(const QString &id, int from) const
{
    for (int row = from; row < m_devices.size(); ++row) {
        if (m_devices.at(row).id == id) {
            return row;
        }
    }
    return -1;
}

void DeviceListModel::setDevices(const QList<Device> &wanted)
{
    const int oldCount = m_devices.size();
    bool changed = false;

    // 1. Drop rows whose device is gone.
    QSet<QString> wantedIds;
    for (const Device &device : wanted) {
        wantedIds.insert(device.id);
    }
    for (int row = m_devices.size() - 1; row >= 0; --row) {
        if (!wantedIds.contains(m_devices.at(row).id)) {
            beginRemoveRows({}, row, row);
            m_devices.removeAt(row);
            endRemoveRows();
            changed = true;
        }
    }

    // 2. Walk the wanted order: move rows whose position changed (only happens when a device
    //    is renamed), insert new ones, and report which roles of the others changed.
    for (int row = 0; row < wanted.size(); ++row) {
        const Device &device = wanted.at(row);
        const int current = rowOf(device.id, row);

        if (current > row) {
            beginMoveRows({}, current, current, {}, row);
            m_devices.move(current, row);
            endMoveRows();
            changed = true;
        }

        if (current < 0) {
            beginInsertRows({}, row, row);
            m_devices.insert(row, device);
            endInsertRows();
            changed = true;
            continue;
        }

        const QList<int> roles = changedRoles(m_devices.at(row), device);
        m_devices[row] = device; // also keeps fields no role shows, e.g. the address
        if (!roles.isEmpty()) {
            Q_EMIT dataChanged(index(row), index(row), roles);
            changed = true;
        }
    }

    if (m_devices.size() != oldCount) {
        Q_EMIT countChanged();
    }
    if (changed) {
        if (DEVICEBATTERY().isDebugEnabled()) {
            QStringList levels;
            for (const Device &device : std::as_const(m_devices)) {
                levels << QStringLiteral("%1 %2%%3").arg(device.name).arg(device.percentage).arg(device.charging ? QStringLiteral(" (charging)") : QString());
            }
            qCDebug(DEVICEBATTERY).noquote() << "Devices:" << levels.join(QStringLiteral(", "));
        }
        Q_EMIT summaryChanged();
    }
}
