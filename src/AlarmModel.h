#ifndef ALARMMODEL_H
#define ALARMMODEL_H

#include "Alarm.h"

#include <QAbstractListModel>
#include <QVariantList>
#include <QVector>

// Alarm storage exposed to QML as a proper list model.
//
// Replaces the old Q_PROPERTY(QVariantList alarms ...) on AlarmManager, which
// rebuilt the whole QVariantList and emitted a single alarmsChanged() on every
// mutation. This model emits fine-grained signals (rowsInserted / rowsRemoved /
// dataChanged / modelReset) so QML only updates the rows that actually changed.
//
// The model keeps alarms in the exact order the old UI displayed them:
// enabled alarms first, then time-of-day ascending (this sorting used to live
// in AlarmListView.qml's refreshSortedAlarms()). A per-alarm sequence number
// preserves the old stable-sort tie order, so the visible list — and the order
// alarms are persisted in after the first save — matches the previous
// behavior exactly.
class AlarmModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)

public:
    enum AlarmRole {
        AlarmIdRole = Qt::UserRole + 1,
        HourRole,
        MinuteRole,
        AlarmEnabledRole,   // NOT named "enabled": would collide with Item.enabled
        NameRole,
        DaysRole,
        WakeModeRole,
        AutoStopDurationRole,
        EnableSoundRole,
        EnableCommandRole,
        CommandRole,
        SoundFileRole
    };
    Q_ENUM(AlarmRole)

    explicit AlarmModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool validRow(int row) const;

    // Mutations — each emits the matching model signal and changed().
    void addAlarm(const Alarm &alarm);
    void removeAlarm(int row);
    void updateAlarm(int row, const Alarm &alarm);
    void setEnabled(int row, bool enabled);
    void resetFromList(const QVector<Alarm> &alarms);   // used on load

    // Convenience accessors (C++ and QML — get() keeps the QVariantMap shape
    // the old alarmManager.alarms entries had).
    const Alarm &at(int row) const;
    QVector<Alarm> alarms() const;
    int indexOfId(const QString &id) const;
    Q_INVOKABLE QVariant get(int row) const;

signals:
    // Emitted after any mutation so AlarmManager can persist to alarms.json.
    void changed();
    void countChanged();

private:
    struct Entry {
        Alarm alarm;
        int seq = 0;    // insertion order — stable-sort tie breaker (old behavior)
    };

    // Sort key: enabled first, then time-of-day, then insertion sequence —
    // mirrors AlarmListView.qml's old refreshSortedAlarms() comparator.
    static bool lessThan(const Entry &a, const Entry &b);
    static QList<int> allRoles();
    // Move `row` to its sorted position (beginMoveRows) if it is out of order.
    void repositionRow(int row);

    QVector<Entry> m_entries;
    int m_nextSeq = 0;
};

#endif // ALARMMODEL_H
