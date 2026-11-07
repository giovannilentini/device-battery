// SPDX-License-Identifier: MIT
#include "devicefilter.h"

#include "devicelistmodel.h"

DeviceFilter::DeviceFilter(QObject *parent)
    : QSortFilterProxyModel(parent)
{
}

void DeviceFilter::setSourceModel(QAbstractItemModel *model)
{
    if (m_source) {
        disconnect(m_source, nullptr, this, nullptr);
    }
    m_source = qobject_cast<DeviceListModel *>(model);
    QSortFilterProxyModel::setSourceModel(m_source);
    if (m_source) {
        connect(m_source, &DeviceListModel::summaryChanged, this, &DeviceFilter::summaryChanged);
    }
    Q_EMIT summaryChanged();
}

QVariant DeviceFilter::data(const QModelIndex &index, int role) const
{
    if ((role == Qt::DisplayRole || role == DeviceListModel::NameRole) && m_source && index.isValid()) {
        const QString name = customName(m_source->devices().at(mapToSource(index).row()));
        if (!name.isEmpty()) {
            return name;
        }
    }
    return QSortFilterProxyModel::data(index, role);
}

bool DeviceFilter::filterAcceptsRow(int sourceRow, const QModelIndex &sourceParent) const
{
    return !sourceParent.isValid() && m_source && sourceRow < m_source->devices().size() && accepts(m_source->devices().at(sourceRow));
}

bool DeviceFilter::accepts(const Device &device) const
{
    return (m_showDisconnected || device.connected) && !m_hiddenKeys.contains(deviceKey(device));
}

QString DeviceFilter::customName(const Device &device) const
{
    return m_customNames.value(deviceKey(device)).toString().trimmed();
}

QStringList DeviceFilter::hiddenKeys() const
{
    return m_hiddenKeys;
}

void DeviceFilter::setHiddenKeys(const QStringList &keys)
{
    if (m_hiddenKeys != keys) {
        changeFilter([&] {
            m_hiddenKeys = keys;
        });
        Q_EMIT hiddenKeysChanged();
    }
}

QVariantMap DeviceFilter::customNames() const
{
    return m_customNames;
}

void DeviceFilter::setCustomNames(const QVariantMap &names)
{
    if (m_customNames == names) {
        return;
    }
    m_customNames = names;
    Q_EMIT customNamesChanged();
    if (rowCount() > 0) {
        Q_EMIT dataChanged(index(0, 0), index(rowCount() - 1, 0), {Qt::DisplayRole, DeviceListModel::NameRole});
    }
    Q_EMIT summaryChanged();
}

bool DeviceFilter::showDisconnected() const
{
    return m_showDisconnected;
}

void DeviceFilter::setShowDisconnected(bool show)
{
    if (m_showDisconnected != show) {
        changeFilter([&] {
            m_showDisconnected = show;
        });
        Q_EMIT showDisconnectedChanged();
    }
}

void DeviceFilter::changeFilter(const std::function<void()> &apply)
{
    // Qt 6.10 replaced invalidateRowsFilter() by a pair of calls around the change.
#if QT_VERSION >= QT_VERSION_CHECK(6, 10, 0)
    beginFilterChange();
    apply();
    endFilterChange(QSortFilterProxyModel::Direction::Rows);
#else
    apply();
    invalidateRowsFilter();
#endif
    Q_EMIT summaryChanged();
}

int DeviceFilter::count() const
{
    return rowCount();
}

QList<Device> DeviceFilter::shownDevices() const
{
    QList<Device> shown;
    if (!m_source) {
        return shown;
    }
    for (Device device : m_source->devices()) {
        if (accepts(device)) {
            if (const QString name = customName(device); !name.isEmpty()) {
                device.name = name;
            }
            shown.append(device);
        }
    }
    return shown;
}

QVariantList DeviceFilter::summary() const
{
    return devicesToVariant(shownDevices());
}

int DeviceFilter::lowestIndex() const
{
    return lowestDeviceIndex(shownDevices());
}
