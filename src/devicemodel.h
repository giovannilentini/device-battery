// SPDX-License-Identifier: MIT
#pragma once

#include "upowersource.h"

#include <QAbstractListModel>
#include <QQmlEngine>

// Every device with a battery, merged from all sources and sorted by name.
//
// A QML singleton, so all widget instances (panel, system tray, desktop) share one
// backend. Updates are applied as minimal row
// inserts/removes/changes, so the views never rebuild their delegates.
class DeviceModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Plain snapshot of every device for the tooltip and panel icon:
    // [{name, iconName, percentage, charging}, ...]
    Q_PROPERTY(QVariantList summary READ summary NOTIFY summaryChanged)

public:
    enum Role {
        NameRole = Qt::UserRole + 1,
        IconNameRole,
        PercentageRole, // 0-100, or -1 when unknown
        ChargingRole,
        PartsRole, // [{component: "left"|"right"|"case", percentage, charging}, ...]
    };
    Q_ENUM(Role)

    explicit DeviceModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    QVariantList summary() const;

Q_SIGNALS:
    void countChanged();
    void summaryChanged();

private:
    QList<Device> mergedDevices() const;
    void sync();
    int rowOf(const QString &id, int from = 0) const;

    UPowerSource m_upower;
    QList<Device> m_devices;
};
