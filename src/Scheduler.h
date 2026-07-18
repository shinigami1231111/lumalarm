#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <QSet>
#include "AlarmManager.h"

class Scheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int nextAlarmSeconds READ nextAlarmSeconds NOTIFY nextAlarmChanged)

public:
    explicit Scheduler(AlarmManager *manager, QObject *parent = nullptr);

    int nextAlarmSeconds() const;

    // Returns seconds until the next enabled alarm, or -1 if none
    Q_INVOKABLE int secondsUntilNextAlarm() const;

    // Creates a temporary snooze alarm at now + snoozeMinutes (not persisted to disk)
    Q_INVOKABLE void snooze(int snoozeMinutes = 5);

signals:
    void alarmTriggered(int alarmIndex);
    void nextAlarmChanged();
    void countdownUpdated(int seconds);
    // Emitted ~90s before alarm for soundscape start
    void soundscapeStarting(int alarmIndex);

private slots:
    void onCheckTimer();

private:
    AlarmManager *m_manager;
    QTimer *m_checkTimer;
    int m_lastTriggeredMin = -1;
    // Track which alarms have had their soundscape triggered
    QSet<int> m_soundscapeFired;
};

#endif // SCHEDULER_H
