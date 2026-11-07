// SPDX-License-Identifier: MIT
#include "devicefilter.h"
#include "devicelistmodel.h"

#include <QAbstractItemModelTester>
#include <QSignalSpy>
#include <QTest>

namespace
{
Device device(const QString &id, const QString &name, int percentage, const QString &address = {})
{
    Device device;
    device.id = id;
    device.name = name;
    device.percentage = percentage;
    device.address = address;
    return device;
}

QStringList namesIn(const QAbstractItemModel &model)
{
    QStringList names;
    for (int row = 0; row < model.rowCount(); ++row) {
        names << model.index(row, 0).data(DeviceListModel::NameRole).toString();
    }
    return names;
}

const QString AirPodsAddress = QStringLiteral("38:C4:3A:7B:06:85");
}

class DeviceFilterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void init()
    {
        Device gone = device(QStringLiteral("pad"), QStringLiteral("Gamepad"), 3);
        gone.connected = false;
        gone.lastSeen = 1;
        m_source.setDevices({
            device(QStringLiteral("aap"), QStringLiteral("AirPods"), 60, AirPodsAddress),
            device(QStringLiteral("mouse"), QStringLiteral("Mouse"), 10),
            gone,
        });
    }

    void showsEverythingByDefault()
    {
        DeviceFilter filter;
        QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
        filter.setSourceModel(&m_source);

        QCOMPARE(namesIn(filter), (QStringList{QStringLiteral("AirPods"), QStringLiteral("Mouse"), QStringLiteral("Gamepad")}));
        QCOMPARE(filter.count(), 3);
        QCOMPARE(filter.lowestIndex(), 1); // the disconnected gamepad never counts
    }

    void hidesByKey()
    {
        DeviceFilter filter;
        QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
        filter.setSourceModel(&m_source);
        QSignalSpy summary(&filter, &DeviceFilter::summaryChanged);

        filter.setHiddenKeys({QStringLiteral("name:Mouse"), QStringLiteral("bt:") + AirPodsAddress});
        QCOMPARE(namesIn(filter), QStringList{QStringLiteral("Gamepad")});
        QCOMPARE(filter.summary().size(), 1);
        QCOMPARE(filter.lowestIndex(), -1);
        QCOMPARE(summary.size(), 1);

        // The mouse comes back under another UPower id: still hidden, as the key is its name.
        m_source.setDevices({device(QStringLiteral("mouse-2"), QStringLiteral("Mouse"), 10)});
        QCOMPARE(filter.count(), 0);
    }

    void leavesOutDisconnectedDevices()
    {
        DeviceFilter filter;
        QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
        filter.setSourceModel(&m_source);

        filter.setShowDisconnected(false);
        QCOMPARE(namesIn(filter), (QStringList{QStringLiteral("AirPods"), QStringLiteral("Mouse")}));

        // A device that disconnects disappears from the filter as its row changes.
        Device mouse = device(QStringLiteral("mouse"), QStringLiteral("Mouse"), 10);
        mouse.connected = false;
        mouse.lastSeen = 2;
        m_source.setDevices({device(QStringLiteral("aap"), QStringLiteral("AirPods"), 60, AirPodsAddress), mouse});
        QCOMPARE(namesIn(filter), QStringList{QStringLiteral("AirPods")});
    }

    void renamesDevices()
    {
        DeviceFilter filter;
        QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
        filter.setSourceModel(&m_source);
        QSignalSpy changed(&filter, &QAbstractItemModel::dataChanged);

        filter.setCustomNames({{QStringLiteral("bt:") + AirPodsAddress, QStringLiteral("My buds")}, {QStringLiteral("name:Mouse"), QStringLiteral("  ")}});
        QCOMPARE(namesIn(filter), (QStringList{QStringLiteral("My buds"), QStringLiteral("Mouse"), QStringLiteral("Gamepad")}));
        QCOMPARE(filter.summary().first().toMap().value(QStringLiteral("name")).toString(), QStringLiteral("My buds"));
        QCOMPARE(changed.size(), 1);
        // The key stays that of the device, so the settings keep finding it.
        QCOMPARE(filter.index(0, 0).data(DeviceListModel::KeyRole).toString(), QStringLiteral("bt:") + AirPodsAddress);
    }

    void followsTheSource()
    {
        DeviceFilter filter;
        QAbstractItemModelTester tester(&filter, QAbstractItemModelTester::FailureReportingMode::QtTest);
        filter.setSourceModel(&m_source);
        filter.setHiddenKeys({QStringLiteral("name:Mouse")});
        QSignalSpy summary(&filter, &DeviceFilter::summaryChanged);

        m_source.setDevices({device(QStringLiteral("kbd"), QStringLiteral("Keyboard"), 5), device(QStringLiteral("mouse"), QStringLiteral("Mouse"), 1)});
        QCOMPARE(namesIn(filter), QStringList{QStringLiteral("Keyboard")});
        QCOMPARE(filter.lowestIndex(), 0); // the hidden mouse is lower but not shown
        QVERIFY(summary.size() >= 1);
    }

private:
    DeviceListModel m_source;
};

QTEST_GUILESS_MAIN(DeviceFilterTest)
#include "devicefiltertest.moc"
