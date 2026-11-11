// SPDX-License-Identifier: MIT
#include "devicefilter.h"

#include "devicelistmodel.h"

#include <algorithm>
#include <iterator>

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
    if (!m_showDisconnected && !device.connected) {
        return false;
    }
    const QString legacyKey = legacyDeviceKey(device);
    return !m_hiddenKeys.contains(deviceKey(device)) && (legacyKey.isEmpty() || !m_hiddenKeys.contains(legacyKey));
}

QString DeviceFilter::customName(const Device &device) const
{
    const QString name = m_customNames.value(deviceKey(device)).toString().trimmed();
    if (!name.isEmpty()) {
        return name;
    }
    const QString legacyKey = legacyDeviceKey(device);
    return legacyKey.isEmpty() ? QString() : m_customNames.value(legacyKey).toString().trimmed();
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
    // The keys come from the devices as they are, not as they were renamed: a device keyed by
    // its name would otherwise get the key of its custom name, which no setting refers to.
    QList<Device> shown;
    if (m_source) {
        std::ranges::copy_if(m_source->devices(), std::back_inserter(shown), [this](const Device &device) {
            return accepts(device);
        });
    }
    QVariantList summary = devicesToVariant(shown);
    for (qsizetype i = 0; i < shown.size(); ++i) {
        if (const QString name = customName(shown.at(i)); !name.isEmpty()) {
            QVariantMap entry = summary.at(i).toMap();
            entry.insert(QStringLiteral("name"), name);
            summary[i] = entry;
        }
    }
    return summary;
}

int DeviceFilter::lowestIndex() const
{
    return lowestDeviceIndex(shownDevices());
}
