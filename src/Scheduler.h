#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <QHash>
#include <QString>
#include "AlarmManager.h"
#include "SchedulerState.h"
#include "WakeManager.h"

class Scheduler : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int nextAlarmSeconds READ nextAlarmSeconds NOTIFY nextAlarmChanged)
    Q_PROPERTY(QString wakeState READ wakeState NOTIFY wakeStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY wakeStateChanged)
    Q_PROPERTY(QString pendingInfo READ pendingInfo NOTIFY wakeStateChanged)

public:
    explicit Scheduler(AlarmManager *manager, WakeManager *wake,
                       const QString &stateFilePath = QString(), QObject *parent = nullptr);

    int nextAlarmSeconds() const;
    // UI state: "idle" | "arming" | "armed" | "failed"
    QString wakeState() const;
    QString lastError() const;
    QString pendingInfo() const;

    Q_INVOKABLE int secondsUntilNextAlarm() const;
    Q_INVOKABLE bool hasPendingWake() const;

    // Program the RTC wake alarm for the next enabled alarm occurrence and
    // enter the verified "armed" state. Persists the occurrence BEFORE any
    // RTC programming so a crash/power loss never loses the scheduled wake.
    // This never powers the machine off.
    Q_INVOKABLE void armWake();

    // Power the machine off — allowed ONLY in the "armed" (armed+verified)
    // state. Refuses otherwise: RTC failure and persistence failure both
    // leave the scheduler in "failed", so this is a hard safety gate.
    Q_INVOKABLE void powerOffMachine();

    // At construction the scheduler recovers any persisted pending wake:
    // compares current time to the scheduled absolute timestamp, classifies
    // the wake (OnTime/Late/VeryLate/Missed), triggers exactly once, and
    // marks the occurrence consumed (idempotent).

signals:
    void alarmTriggered(int alarmIndex);
    void nextAlarmChanged();
    void countdownUpdated(int seconds);
    void systemResumed();
    void wakeStateChanged();

private slots:
    void onCheckTimer();
    void onAlarmsChanged();

private:
    QDateTime computeNextOccurrence(const Alarm &a, const QDateTime &from) const;
    bool persistPending();
    void consumePending(const QString &reason);
    void recoverAfterBoot();
    void setWakeState(const QString &state, const QString &error = QString());

    AlarmManager *m_manager;
    WakeManager *m_wake;
    QTimer *m_checkTimer;
    QDateTime m_lastCheckTime;
    QHash<QString, QDateTime> m_lastTriggeredTime;
    SchedulerStateStore m_store;
    PendingWake m_pending;
    QString m_wakeState;   // "idle", "arming", "armed", "failed"
    QString m_lastError;
};

#endif // SCHEDULER_H