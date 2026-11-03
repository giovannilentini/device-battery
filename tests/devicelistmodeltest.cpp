// SPDX-License-Identifier: MIT
#include "devicelistmodel.h"

#include <QAbstractItemModelTester>
#include <QRandomGenerator>
#include <QSignalSpy>
#include <QTest>

namespace
{
Device device(const QString &id, const QString &name, int percentage = 50)
{
    Device device;
    device.id = id;
    device.name = name;
    device.percentage = percentage;
    return device;
}

QStringList namesIn(const DeviceListModel &model)
{
    QStringList ids;
    for (int row = 0; row < model.rowCount(); ++row) {
        ids << model.index(row).data(DeviceListModel::NameRole).toString();
    }
    return ids;
}
}

class DeviceListModelTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void insertsAndRemovesRows()
    {
        DeviceListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QSignalSpy count(&model, &DeviceListModel::countChanged);

        model.setDevices({device(QStringLiteral("a"), QStringLiteral("A")), device(QStringLiteral("b"), QStringLiteral("B"))});
        QCOMPARE(namesIn(model), (QStringList{QStringLiteral("A"), QStringLiteral("B")}));
        QCOMPARE(count.size(), 1);

        model.setDevices({device(QStringLiteral("b"), QStringLiteral("B"))});
        QCOMPARE(namesIn(model), QStringList{QStringLiteral("B")});
        QCOMPARE(count.size(), 2);
    }

    void updatesRowsInPlace()
    {
        DeviceListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setDevices({device(QStringLiteral("a"), QStringLiteral("A"), 50)});

        QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy count(&model, &DeviceListModel::countChanged);
        QSignalSpy summary(&model, &DeviceListModel::summaryChanged);

        model.setDevices({device(QStringLiteral("a"), QStringLiteral("A"), 40)});

        QCOMPARE(inserted.size(), 0);
        QCOMPARE(removed.size(), 0);
        QCOMPARE(count.size(), 0);
        QCOMPARE(changed.size(), 1);
        QCOMPARE(summary.size(), 1);
        QCOMPARE(model.index(0).data(DeviceListModel::PercentageRole).toInt(), 40);
    }

    // Views re-evaluate only the bindings of the roles listed in dataChanged.
    void reportsOnlyChangedRoles()
    {
        DeviceListModel model;
        Device airPods = device(QStringLiteral("aap"), QStringLiteral("AirPods"), 80);
        airPods.parts = {{BatteryPart::Component::Left, 80, false}};
        model.setDevices({airPods});

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        airPods.percentage = 79;
        airPods.parts[0].percentage = 79;
        model.setDevices({airPods});

        QCOMPARE(changed.size(), 1);
        QCOMPARE(changed.at(0).at(2).value<QList<int>>(), (QList<int>{DeviceListModel::PercentageRole, DeviceListModel::PartsRole}));
    }

    void identicalListEmitsNothing()
    {
        DeviceListModel model;
        const QList<Device> devices{device(QStringLiteral("a"), QStringLiteral("A")), device(QStringLiteral("b"), QStringLiteral("B"))};
        model.setDevices(devices);

        QSignalSpy changed(&model, &QAbstractItemModel::dataChanged);
        QSignalSpy summary(&model, &DeviceListModel::summaryChanged);
        model.setDevices(devices);

        QCOMPARE(changed.size(), 0);
        QCOMPARE(summary.size(), 0);
    }

    // A rename that changes the sort order must move the row, not remove and re-add it,
    // so the delegate (and its animations) survive.
    void movesRenamedRow()
    {
        DeviceListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        model.setDevices({device(QStringLiteral("a"), QStringLiteral("A")), device(QStringLiteral("b"), QStringLiteral("B"))});

        QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
        QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
        model.setDevices({device(QStringLiteral("b"), QStringLiteral("B")), device(QStringLiteral("a"), QStringLiteral("C"))});

        QCOMPARE(namesIn(model), (QStringList{QStringLiteral("B"), QStringLiteral("C")}));
        QCOMPARE(moved.size(), 1);
        QCOMPARE(removed.size(), 0);
    }

    // Applies random lists one after another: whatever the previous state, the model must end
    // up showing exactly the new list, and QAbstractItemModelTester checks every signal.
    void reachesAnyListFromAnyList()
    {
        DeviceListModel model;
        QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
        QRandomGenerator random(7); // fixed seed: failures must be reproducible

        for (int round = 0; round < 500; ++round) {
            QList<Device> wanted;
            for (int id = 0; id < 8; ++id) {
                if (random.bounded(2)) {
                    wanted << device(QString::number(id), QStringLiteral("Device %1").arg(id), random.bounded(-1, 101));
                }
            }
            // Shuffle to also exercise moves.
            for (qsizetype i = wanted.size() - 1; i > 0; --i) {
                wanted.swapItemsAt(i, random.bounded(i + 1));
            }

            model.setDevices(wanted);

            QCOMPARE(model.rowCount(), wanted.size());
            for (int row = 0; row < wanted.size(); ++row) {
                QCOMPARE(model.index(row).data(DeviceListModel::NameRole).toString(), wanted.at(row).name);
                QCOMPARE(model.index(row).data(DeviceListModel::PercentageRole).toInt(), wanted.at(row).percentage);
            }
        }
    }

    void exposesParts()
    {
        DeviceListModel model;
        Device airPods = device(QStringLiteral("aap"), QStringLiteral("AirPods"));
        airPods.parts = {{BatteryPart::Component::Left, 90, false}, {BatteryPart::Component::Case, 30, true}};
        model.setDevices({airPods});

        const QVariantList parts = model.index(0).data(DeviceListModel::PartsRole).toList();
        QCOMPARE(parts.size(), 2);
        QCOMPARE(parts.at(0).toMap().value(QStringLiteral("component")).toString(), QStringLiteral("left"));
        QCOMPARE(parts.at(1).toMap().value(QStringLiteral("percentage")).toInt(), 30);
        QCOMPARE(parts.at(1).toMap().value(QStringLiteral("charging")).toBool(), true);
    }

    void preferredDeviceHidesFallbackWithSameAddress()
    {
        Device airPods = device(QStringLiteral("aap:38:C4"), QStringLiteral("AirPods"));
        airPods.address = QStringLiteral("38:C4");
        Device headset = device(QStringLiteral("upower:headset"), QStringLiteral("AirPods (HFP)"));
        headset.address = QStringLiteral("38:C4");
        Device receiver = device(QStringLiteral("upower:receiver"), QStringLiteral("Mouse")); // no address

        const QList<Device> merged = mergeDevices({airPods}, {headset, receiver});

        QCOMPARE(merged.size(), 2);
        QCOMPARE(merged.at(0).id, airPods.id);
        QCOMPARE(merged.at(1).id, receiver.id);
    }

    void devicesWithoutAddressAreNeverMerged()
    {
        const QList<Device> merged = mergeDevices({device(QStringLiteral("a"), QStringLiteral("A"))}, {device(QStringLiteral("b"), QStringLiteral("B"))});
        QCOMPARE(merged.size(), 2);
    }

    void sortsNaturallyAndCaseInsensitively()
    {
        const QList<Device> merged = mergeDevices({device(QStringLiteral("1"), QStringLiteral("mouse 10")), device(QStringLiteral("2"), QStringLiteral("Mouse 2"))},
                                                  {device(QStringLiteral("3"), QStringLiteral("AirPods"))});
        QCOMPARE(merged.at(0).name, QStringLiteral("AirPods"));
        QCOMPARE(merged.at(1).name, QStringLiteral("Mouse 2"));
        QCOMPARE(merged.at(2).name, QStringLiteral("mouse 10"));
    }
};

QTEST_GUILESS_MAIN(DeviceListModelTest)
#include "devicelistmodeltest.moc"
