// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QAbstractListModel>

// The devices shown in the widget, as a list model for QML.
//
// setDevices() applies a new list as minimal row removes, inserts, moves and changes,
// so the views never rebuild their delegates. Knows nothing about where the devices
// come from (see DeviceModel), which keeps it testable without D-Bus.
class DeviceListModel : public QAbstractListModel
{
    Q_OBJECT

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Plain snapshot of every device for the tooltip and panel icon:
    // [{name, iconName, percentage, coarseLevel, charging, parts, model, noiseControl, connected, lastSeen}, ...], as the roles
    Q_PROPERTY(QVariantList summary READ summary NOTIFY summaryChanged)
    // Index in `summary` of the device closest to running out, or -1 (see lowestDeviceIndex()).
    // Changes together with `summary`.
    Q_PROPERTY(int lowestIndex READ lowestIndex NOTIFY summaryChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        IconNameRole,
        PercentageRole, // 0-100, or -1 when unknown
        ChargingRole,
        PartsRole, // [{component: "left"|"right"|"case", percentage, charging, placement: ""|"ear"|"out"|"case"}, ...]
        CoarseLevelRole, // "" or a rough level ("low", "normal", ...) shown instead of the percentage
        ModelRole, // product name, or "" if unknown or already in the name
        NoiseControlRole, // "" or "off", "cancellation", "transparency", "adaptive"
        ConnectedRole, // false for a recently disconnected device, shown with its last levels
        LastSeenRole, // for a disconnected device: when it was last seen (QDateTime), else invalid
    };
    Q_ENUM(Role)

    explicit DeviceListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QVariantList summary() const;
    int lowestIndex() const;

    // Replaces the content with `devices`, which must already be in display order.
    void setDevices(const QList<Device> &devices);
    const QList<Device> &devices() const;

Q_SIGNALS:
    void countChanged();
    void summaryChanged();

private:
    int rowOf(const QString &id, int from = 0) const;

    QList<Device> m_devices;
};
