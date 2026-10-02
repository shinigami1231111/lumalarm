#include "models/AlarmModel.h"

#include <algorithm>

AlarmModel::AlarmModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int AlarmModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid())
        return 0;
    return static_cast<int>(m_entries.size());
}

QVariant AlarmModel::data(const QModelIndex &idx, int role) const
{
    if (!idx.isValid() || !validRow(idx.row()))
        return QVariant();

    const Alarm &a = m_entries[idx.row()].alarm;
    switch (role) {
    case AlarmIdRole:      return a.id;
    case HourRole:         return a.hour;
    case MinuteRole:       return a.minute;
    case AlarmEnabledRole: return a.enabled;
    case NameRole:         return a.name;
    case DaysRole: {
        QVariantList days;
        for (bool d : a.days)
            days.append(d);
        return days;
    }
    case WakeModeRole:           return a.wakeMode;
    case AutoStopDurationRole:   return a.autoStopDuration;
    case EnableSoundRole:        return a.enableSound;
    case EnableCommandRole:      return a.enableCommand;
    case CommandRole:            return a.command;
    case SoundFileRole:          return a.soundFile;
    default:                     return QVariant();
    }
}

QHash<int, QByteArray> AlarmModel::roleNames() const
{
    return {
        { AlarmIdRole,            "alarmId" },
        { HourRole,               "hour" },
        { MinuteRole,             "minute" },
        { AlarmEnabledRole,       "alarmEnabled" },
        { NameRole,               "name" },
        { DaysRole,               "days" },
        { WakeModeRole,           "wakeMode" },
        { AutoStopDurationRole,   "autoStopDuration" },
        { EnableSoundRole,        "enableSound" },
        { EnableCommandRole,      "enableCommand" },
        { CommandRole,            "command" },
        { SoundFileRole,          "soundFile" },
    };
}

int AlarmModel::count() const
{
    return static_cast<int>(m_entries.size());
}

bool AlarmModel::validRow(int row) const
{
    return row >= 0 && row < m_entries.size();
}

void AlarmModel::addAlarm(const Alarm &alarm)
{
    // Insert at the sorted position the old stable sort produced: a newly
    // appended alarm lands after all equal-key alarms (its seq is the largest).
    const Entry entry{alarm, m_nextSeq++};
    const auto it = std::upper_bound(m_entries.begin(), m_entries.end(), entry, &AlarmModel::lessThan);
    const int row = int(it - m_entries.begin());

    beginInsertRows(QModelIndex(), row, row);
    m_entries.insert(row, entry);
    endInsertRows();

    emit countChanged();
    emit changed();
}

void AlarmModel::removeAlarm(int row)
{
    if (!validRow(row))
        return;

    beginRemoveRows(QModelIndex(), row, row);
    m_entries.removeAt(row);
    endRemoveRows();

    emit countChanged();
    emit changed();
}

void AlarmModel::updateAlarm(int row, const Alarm &alarm)
{
    if (!validRow(row))
        return;

    m_entries[row].alarm = alarm;   // seq preserved — insertion order is stable
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, allRoles());
    repositionRow(row);             // may have changed enabled/time -> re-sort
    emit changed();
}

void AlarmModel::setEnabled(int row, bool enabled)
{
    if (!validRow(row))
        return;

    m_entries[row].alarm.enabled = enabled;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx, { AlarmEnabledRole });
    repositionRow(row);             // toggling moves the row between sections
    emit changed();
}

void AlarmModel::resetFromList(const QVector<Alarm> &alarms)
{
    const int oldCount = static_cast<int>(m_entries.size());

    beginResetModel();
    m_entries.clear();
    m_nextSeq = 0;
    for (const Alarm &a : alarms)
        m_entries.append(Entry{ a, m_nextSeq++ });
    std::sort(m_entries.begin(), m_entries.end(), &AlarmModel::lessThan);
    endResetModel();

    if (m_entries.size() != oldCount)
        emit countChanged();
    emit changed();
}

const Alarm &AlarmModel::at(int row) const
{
    static const Alarm fallback;
    return validRow(row) ? m_entries[row].alarm : fallback;
}

QVector<Alarm> AlarmModel::alarms() const
{
    QVector<Alarm> list;
    list.reserve(m_entries.size());
    for (const Entry &e : m_entries)
        list.append(e.alarm);
    return list;
}

int AlarmModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries[i].alarm.id == id)
            return i;
    }
    return -1;
}

QVariant AlarmModel::get(int row) const
{
    if (!validRow(row))
        return QVariant();   // undefined in QML — same guard the old list code used
    return m_entries[row].alarm.toVariantMap();
}

bool AlarmModel::lessThan(const Entry &a, const Entry &b)
{
    // Mirrors AlarmListView.qml's old refreshSortedAlarms() comparator:
    // enabled first, then time-of-day ascending; insertion sequence keeps the
    // old stable-sort tie order.
    if (a.alarm.enabled != b.alarm.enabled)
        return a.alarm.enabled;
    const int ta = a.alarm.hour * 60 + a.alarm.minute;
    const int tb = b.alarm.hour * 60 + b.alarm.minute;
    if (ta != tb)
        return ta < tb;
    return a.seq < b.seq;
}

QList<int> AlarmModel::allRoles()
{
    return {
        AlarmIdRole, HourRole, MinuteRole, AlarmEnabledRole, NameRole, DaysRole,
        WakeModeRole, AutoStopDurationRole, EnableSoundRole, EnableCommandRole,
        CommandRole, SoundFileRole
    };
}

void AlarmModel::repositionRow(int row)
{
    if (!validRow(row))
        return;

    const Entry entry = m_entries[row];

    // Final sorted index = number of *other* entries sorting before this one
    // (the rest of the model is already sorted; only `row` can be out of place).
    int target = 0;
    for (int i = 0; i < m_entries.size(); ++i) {
        if (i != row && lessThan(m_entries[i], entry))
            ++target;
    }
    if (target == row)
        return;

    // Qt same-parent move convention: destination is the pre-move index the
    // items are inserted before, so moving down shifts by one.
    const int destination = target > row ? target + 1 : target;
    beginMoveRows(QModelIndex(), row, row, QModelIndex(), destination);
    m_entries.move(row, target);
    endMoveRows();
}
