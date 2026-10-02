#include <QtTest>

#include "models/AlarmModel.h"

// ============================================================================
// Unit tests for AlarmModel, the QAbstractListModel that replaced the old
// Q_PROPERTY(QVariantList alarms ...) exposure on AlarmManager.
// Covers: rowCount, fine-grained insert/remove/update signals, data() per
// role, the preserved display sort order (enabled first, then time-of-day,
// stable tie order), count property, and get() map shape.
// ============================================================================

class TestAlarmModel : public QObject
{
    Q_OBJECT

private slots:
    void initialRowCountIsZero();
    void addAlarmEmitsRowsInsertedOnce();
    void removeAlarmEmitsRowsRemovedOnce();
    void updateAlarmEmitsDataChangedForThatRowOnly();
    void dataReturnsCorrectValueForEachRole();
    void sortingMatchesOldDisplayOrder();
    void tieOrderIsStableAcrossToggles();
    void setEnabledNotifiesOnlyEnabledRole();
    void resetFromListReloadsAllRows();
    void countPropertyTracksRows();
    void getReturnsSameMapShapeAsOldAlarmsProperty();
};

static Alarm makeAlarm(int hour, int minute, bool enabled, const QString &name = QString())
{
    Alarm a;
    a.hour = hour;
    a.minute = minute;
    a.enabled = enabled;
    a.name = name;
    return a;
}

void TestAlarmModel::initialRowCountIsZero()
{
    AlarmModel model;
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(model.count(), 0);
    QVERIFY(model.get(0).isValid() == false);
}

void TestAlarmModel::addAlarmEmitsRowsInsertedOnce()
{
    AlarmModel model;
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QVERIFY(inserted.isValid());

    model.addAlarm(makeAlarm(7, 0, true));

    QCOMPARE(model.rowCount(), 1);
    QCOMPARE(inserted.count(), 1);
    // parent invalid (top level), inserted range row 0..0
    QVERIFY(!inserted.at(0).at(0).value<QModelIndex>().isValid());
    QCOMPARE(inserted.at(0).at(1).toInt(), 0);
    QCOMPARE(inserted.at(0).at(2).toInt(), 0);
}

void TestAlarmModel::removeAlarmEmitsRowsRemovedOnce()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(7, 0, true));
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QVERIFY(removed.isValid());

    model.removeAlarm(0);

    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(removed.count(), 1);
    QVERIFY(!removed.at(0).at(0).value<QModelIndex>().isValid());
    QCOMPARE(removed.at(0).at(1).toInt(), 0);
    QCOMPARE(removed.at(0).at(2).toInt(), 0);

    // Invalid row is a no-op (matches old AlarmManager guard).
    model.removeAlarm(5);
    QCOMPARE(removed.count(), 1);
}

void TestAlarmModel::updateAlarmEmitsDataChangedForThatRowOnly()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(7, 0, true, "first"));
    model.addAlarm(makeAlarm(9, 0, true, "second"));

    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy inserted(&model, &QAbstractItemModel::rowsInserted);
    QSignalSpy removed(&model, &QAbstractItemModel::rowsRemoved);
    QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);
    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);

    // Same time + enabled state -> only the data changes, no re-sort, no move.
    Alarm updated = model.at(0);
    updated.name = "renamed";
    model.updateAlarm(0, updated);

    QCOMPARE(dataChanged.count(), 1);
    QCOMPARE(dataChanged.at(0).at(0).value<QModelIndex>().row(), 0);
    QCOMPARE(dataChanged.at(0).at(1).value<QModelIndex>().row(), 0);
    QVERIFY(dataChanged.at(0).at(2).value<QList<int>>().contains(AlarmModel::NameRole));
    QCOMPARE(model.at(0).name, QString("renamed"));

    // Nothing but dataChanged may fire.
    QCOMPARE(inserted.count(), 0);
    QCOMPARE(removed.count(), 0);
    QCOMPARE(moved.count(), 0);
    QCOMPARE(reset.count(), 0);

    // Invalid row is a no-op.
    model.updateAlarm(9, updated);
    QCOMPARE(dataChanged.count(), 1);
}

void TestAlarmModel::dataReturnsCorrectValueForEachRole()
{
    AlarmModel model;
    Alarm a = makeAlarm(6, 42, false, "Work");
    a.id = "fixed-id";
    a.days = { true, false, true, false, false, true, false };
    a.wakeMode = "rtc";
    a.autoStopDuration = 60;
    a.enableSound = false;
    a.enableCommand = true;
    a.command = "notify-send hi";
    a.soundFile = "beep.wav";
    model.addAlarm(a);

    const QModelIndex idx = model.index(0);
    QVERIFY(idx.isValid());
    QCOMPARE(model.data(idx, AlarmModel::AlarmIdRole).toString(), QString("fixed-id"));
    QCOMPARE(model.data(idx, AlarmModel::HourRole).toInt(), 6);
    QCOMPARE(model.data(idx, AlarmModel::MinuteRole).toInt(), 42);
    QCOMPARE(model.data(idx, AlarmModel::AlarmEnabledRole).toBool(), false);
    QCOMPARE(model.data(idx, AlarmModel::NameRole).toString(), QString("Work"));
    QCOMPARE(model.data(idx, AlarmModel::WakeModeRole).toString(), QString("rtc"));
    QCOMPARE(model.data(idx, AlarmModel::AutoStopDurationRole).toInt(), 60);
    QCOMPARE(model.data(idx, AlarmModel::EnableSoundRole).toBool(), false);
    QCOMPARE(model.data(idx, AlarmModel::EnableCommandRole).toBool(), true);
    QCOMPARE(model.data(idx, AlarmModel::CommandRole).toString(), QString("notify-send hi"));
    QCOMPARE(model.data(idx, AlarmModel::SoundFileRole).toString(), QString("beep.wav"));

    const QVariantList days = model.data(idx, AlarmModel::DaysRole).toList();
    QCOMPARE(days.size(), 7);
    QCOMPARE(days.at(0).toBool(), true);
    QCOMPARE(days.at(1).toBool(), false);
    QCOMPARE(days.at(2).toBool(), true);
    QCOMPARE(days.at(5).toBool(), true);
    QCOMPARE(days.at(6).toBool(), false);

    // Unknown role and out-of-range index yield nothing.
    QVERIFY(!model.data(idx, Qt::UserRole + 99).isValid());
    QVERIFY(!model.data(model.index(5), AlarmModel::HourRole).isValid());
    // List models have no children.
    QCOMPARE(model.rowCount(idx), 0);

    // roleNames expose what the QML delegate declares as required properties.
    const QHash<int, QByteArray> roles = model.roleNames();
    const QList<QByteArray> expected{
        "alarmId", "hour", "minute", "alarmEnabled", "name", "days",
        "wakeMode", "autoStopDuration", "enableSound", "enableCommand",
        "command", "soundFile"
    };
    for (const QByteArray &roleName : expected)
        QVERIFY2(roles.values().contains(roleName), roleName.constData());
}

void TestAlarmModel::sortingMatchesOldDisplayOrder()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(9, 0, true));
    model.addAlarm(makeAlarm(6, 0, true));
    model.addAlarm(makeAlarm(6, 30, false));
    model.addAlarm(makeAlarm(7, 15, true));

    // Old refreshSortedAlarms(): enabled first, then time-of-day ascending.
    QCOMPARE(model.rowCount(), 4);
    QCOMPARE(model.at(0).hour, 6);  QCOMPARE(model.at(0).minute, 0);   QVERIFY(model.at(0).enabled);
    QCOMPARE(model.at(1).hour, 7);  QCOMPARE(model.at(1).minute, 15);  QVERIFY(model.at(1).enabled);
    QCOMPARE(model.at(2).hour, 9);  QCOMPARE(model.at(2).minute, 0);   QVERIFY(model.at(2).enabled);
    QCOMPARE(model.at(3).hour, 6);  QCOMPARE(model.at(3).minute, 30);  QVERIFY(!model.at(3).enabled);
}

void TestAlarmModel::tieOrderIsStableAcrossToggles()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(7, 0, true, "A"));
    model.addAlarm(makeAlarm(7, 0, true, "B"));

    // Equal key -> old stable sort kept insertion order: A before B.
    QCOMPARE(model.at(0).name, QString("A"));
    QCOMPARE(model.at(1).name, QString("B"));

    // Toggle A off -> it moves into the disabled section (B stays first).
    model.setEnabled(0, false);
    QCOMPARE(model.at(0).name, QString("B"));
    QVERIFY(model.at(0).enabled);
    QCOMPARE(model.at(1).name, QString("A"));
    QVERIFY(!model.at(1).enabled);

    // Toggle A back on -> insertion order tie-break restores A first,
    // exactly like the old stable re-sort over the storage list.
    model.setEnabled(1, true);
    QCOMPARE(model.at(0).name, QString("A"));
    QCOMPARE(model.at(1).name, QString("B"));
}

void TestAlarmModel::setEnabledNotifiesOnlyEnabledRole()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(7, 0, true));
    QSignalSpy dataChanged(&model, &QAbstractItemModel::dataChanged);
    QSignalSpy moved(&model, &QAbstractItemModel::rowsMoved);

    model.setEnabled(0, false);

    QCOMPARE(dataChanged.count(), 1);
    QCOMPARE(dataChanged.at(0).at(0).value<QModelIndex>().row(), 0);
    const QList<int> roles = dataChanged.at(0).at(2).value<QList<int>>();
    QCOMPARE(roles.count(), 1);
    QCOMPARE(roles.first(), int(AlarmModel::AlarmEnabledRole));
    QVERIFY(!model.at(0).enabled);
    // Single alarm: no reorder possible.
    QCOMPARE(moved.count(), 0);

    model.setEnabled(5, true);   // invalid row: no-op
    QCOMPARE(dataChanged.count(), 1);
}

void TestAlarmModel::resetFromListReloadsAllRows()
{
    AlarmModel model;
    model.addAlarm(makeAlarm(7, 0, true));
    model.addAlarm(makeAlarm(8, 0, true));

    QSignalSpy reset(&model, &QAbstractItemModel::modelReset);
    QSignalSpy countChanged(&model, &AlarmModel::countChanged);
    QSignalSpy changed(&model, &AlarmModel::changed);

    QVector<Alarm> loaded;
    loaded.append(makeAlarm(6, 30, false, "loaded-1"));
    loaded.append(makeAlarm(23, 59, true, "loaded-2"));
    model.resetFromList(loaded);

    QCOMPARE(reset.count(), 1);
    QCOMPARE(model.rowCount(), 2);
    // loaded-1 is disabled, loaded-2 enabled -> enabled first:
    QCOMPARE(model.at(0).name, QString("loaded-2"));
    QCOMPARE(model.at(1).name, QString("loaded-1"));
    QVERIFY(!model.at(1).enabled);
    QCOMPARE(countChanged.count(), 0);   // 2 -> 2, count unchanged
    QVERIFY(changed.count() >= 1);       // AlarmManager persists on this

    model.resetFromList({});
    QCOMPARE(model.rowCount(), 0);
    QCOMPARE(reset.count(), 2);
    QCOMPARE(countChanged.count(), 1);
}

void TestAlarmModel::countPropertyTracksRows()
{
    AlarmModel model;
    QSignalSpy countChanged(&model, &AlarmModel::countChanged);

    model.addAlarm(makeAlarm(7, 0, true));
    QCOMPARE(model.count(), 1);
    QCOMPARE(countChanged.count(), 1);

    model.addAlarm(makeAlarm(8, 0, true));
    QCOMPARE(model.count(), 2);
    QCOMPARE(countChanged.count(), 2);

    model.removeAlarm(0);
    QCOMPARE(model.count(), 1);
    QCOMPARE(countChanged.count(), 3);
}

void TestAlarmModel::getReturnsSameMapShapeAsOldAlarmsProperty()
{
    AlarmModel model;
    Alarm a = makeAlarm(7, 30, true, "Shape");
    a.days = { true, true, false, false, false, false, false };
    model.addAlarm(a);

    const QVariant value = model.get(0);
    QVERIFY(value.isValid());
    QCOMPARE(value.typeId(), int(QMetaType::QVariantMap));
    const QVariantMap map = value.toMap();

    // Same keys the old Alarm::toVariantMap()/alarmManager.alarms produced —
    // AlarmEditor.loadAlarm() and main.qml's onAlarmTriggered() rely on them.
    const QStringList expectedKeys{
        "id", "hour", "minute", "days", "wakeMode", "autoStopDuration",
        "enableSound", "enableCommand", "command", "enabled", "soundFile", "name"
    };
    for (const QString &key : expectedKeys)
        QVERIFY2(map.contains(key), qPrintable("missing key: " + key));

    QCOMPARE(map.value("hour").toInt(), 7);
    QCOMPARE(map.value("minute").toInt(), 30);
    QCOMPARE(map.value("enabled").toBool(), true);
    QCOMPARE(map.value("name").toString(), QString("Shape"));
    QCOMPARE(map.value("days").toList().size(), 7);

    // Out-of-range row -> undefined in QML (invalid QVariant here).
    QVERIFY(!model.get(3).isValid());
}

QTEST_GUILESS_MAIN(TestAlarmModel)
#include "tst_alarmmodel.moc"
