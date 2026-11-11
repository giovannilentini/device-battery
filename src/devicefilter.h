// SPDX-License-Identifier: MIT
#pragma once

#include "device.h"

#include <QSortFilterProxyModel>
#include <QStringList>
#include <QVariantMap>

#include <functional>

class DeviceListModel;

// The devices one widget instance shows: the shared DeviceListModel with that instance's
// per-device settings applied. Hidden devices are left out, renamed ones get their custom
// name, and disconnected ones can be left out entirely.
//
// Settings refer to devices by their deviceKey(), which stays the same across reconnections,
// or by their legacyDeviceKey().
class DeviceFilter : public QSortFilterProxyModel
{
    Q_OBJECT

    // Keys of the devices not to show.
    Q_PROPERTY(QStringList hiddenKeys READ hiddenKeys WRITE setHiddenKeys NOTIFY hiddenKeysChanged)
    // Key -> name to show instead of the device's own. Empty names are ignored.
    Q_PROPERTY(QVariantMap customNames READ customNames WRITE setCustomNames NOTIFY customNamesChanged)
    Q_PROPERTY(bool showDisconnected READ showDisconnected WRITE setShowDisconnected NOTIFY showDisconnectedChanged)

    // As DeviceListModel's, for the devices shown.
    Q_PROPERTY(int count READ count NOTIFY summaryChanged)
    Q_PROPERTY(QVariantList summary READ summary NOTIFY summaryChanged)
    Q_PROPERTY(int lowestIndex READ lowestIndex NOTIFY summaryChanged)

public:
    explicit DeviceFilter(QObject *parent = nullptr);

    // Must be a DeviceListModel; anything else shows nothing.
    void setSourceModel(QAbstractItemModel *model) override;
    QVariant data(const QModelIndex &index, int role) const override;

    QStringList hiddenKeys() const;
    void setHiddenKeys(const QStringList &keys);
    QVariantMap customNames() const;
    void setCustomNames(const QVariantMap &names);
    bool showDisconnected() const;
    void setShowDisconnected(bool show);

    int count() const;
    QVariantList summary() const;
    int lowestIndex() const;

    // The devices shown, in display order and with their custom names.
    QList<Device> shownDevices() const;

Q_SIGNALS:
    void hiddenKeysChanged();
    void customNamesChanged();
    void showDisconnectedChanged();
    void summaryChanged();

protected:
    bool filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const override;

private:
    bool accepts(const Device &device) const;
    QString customName(const Device &device) const;
    void changeFilter(const std::function<void()> &apply);

    DeviceListModel *m_source = nullptr;
    QStringList m_hiddenKeys;
    QVariantMap m_customNames;
    bool m_showDisconnected = true;
};
