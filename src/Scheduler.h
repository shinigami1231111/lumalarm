#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <QObject>
#include <QTimer>
#include <QTime>
#include <QDateTime>
#include <QSet>
#include <QHash>
#include <QString>
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

    // Starts a precise snooze timer for the alarm identified by alarmId.
    // The timer fires after snoozeMinutes and re-triggers the alarm (if still
    // present and enabled). Snoozes are persisted so they survive restarts.
    Q_INVOKABLE void startSnoozeTimer(const QString &alarmId, int snoozeMinutes);

    // Cancels any pending snooze for the given alarm id.
    Q_INVOKABLE void cancelSnooze(const QString &alarmId);

    // Re-arm snooze timers from disk (call once after construction).
    void restoreSnoozes();

signals:
    void alarmTriggered(int alarmIndex);
    void nextAlarmChanged();
    void countdownUpdated(int seconds);
    // Emitted ~90s before alarm for soundscape start
    void soundscapeStarting(int alarmIndex);
    // Emitted when system resumes from suspend (detected via time gap)
    void systemResumed();

private slots:
    void onCheckTimer();

private:
    struct SnoozeEntry {
        QString alarmId;
        QDateTime fireAt;
        QTimer *timer = nullptr;
    };

    void armSnooze(const QString &alarmId, const QDateTime &fireAt, bool persist);
    void fireSnooze(const QString &alarmId);
    void loadSnoozes();
    void saveSnoozes();
    QString snoozeFilePath() const;

    AlarmManager *m_manager;
    QTimer *m_checkTimer;
    int m_lastTriggeredMin = -1;
    QDateTime m_lastCheckTime = QDateTime::currentDateTime();
    // Track which alarms have had their soundscape triggered
    QSet<int> m_soundscapeFired;
    // Active snooze timers, keyed by alarm id
    QHash<QString, SnoozeEntry> m_snoozes;
};

#endif // SCHEDULER_H
