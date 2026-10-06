#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include "models/Alarm.h"
#include "models/AlarmModel.h"

#include <QDir>
#include <QFile>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <QVariantMap>
#include <QVector>

// Owns the alarm storage and persistence (alarms.json).
//
// The alarm list itself now lives in an AlarmModel (a QAbstractListModel)
// exposed to QML as `alarmManager.model`, replacing the old
// Q_PROPERTY(QVariantList alarms ...) which rebuilt the entire list on every
// mutation. QML consumes fine-grained model signals instead.
//
// Public surface kept intact:
//   - addAlarm / removeAlarm / updateAlarm / indexOfId / loadFromFile /
//     saveToFile keep their names and QVariantMap-based signatures.
//   - alarmsChanged() still fires after every mutation — Scheduler.cpp
//     (2 connections) and main.qml's Connections block rely on it.
//   - alarmList() / setAlarms() keep their QVector<Alarm> signatures for
//     Scheduler, main.cpp and tst_schedulerlogic.
class AlarmManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AlarmModel *model READ model CONSTANT)

public:
    explicit AlarmManager(QObject *parent = nullptr);

    AlarmModel *model() const;

    Q_INVOKABLE void addAlarm(const QVariantMap &alarm);
    Q_INVOKABLE void removeAlarm(int index);
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE void updateAlarm(int index, const QVariantMap &alarm);
    // Id-based enable/disable for the list toggle: resolves the row by alarm
    // id (immune to the enabled-first re-sort) and flips the flag in place,
    // so the alarm's id — and therefore the QML selection and Scheduler's
    // PendingWake.alarmId reference — survives the toggle.
    Q_INVOKABLE void setAlarmEnabled(const QString &id, bool enabled);
    Q_INVOKABLE void loadFromFile();
    Q_INVOKABLE void saveToFile();

    QVector<Alarm> alarmList() const;
    void setAlarms(const QVector<Alarm> &alarms);
    QString alarmsFilePath() const;

signals:
    void alarmsChanged();
    void persistenceError(const QString &message);

private:
    AlarmModel *m_model = nullptr;
    QString m_filePath;
    // True while (re)loading the model so the changed() handler does not write
    // the file straight back — matching the pre-refactor load, which only
    // saved when the alarms file was missing entirely.
    bool m_suppressPersist = false;
};

#endif // ALARMMANAGER_H
