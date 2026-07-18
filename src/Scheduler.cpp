#include "Scheduler.h"
#include <QDebug>

Scheduler::Scheduler(AlarmManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_checkTimer(new QTimer(this))
{
    connect(m_checkTimer, &QTimer::timeout, this, &Scheduler::onCheckTimer);
    m_checkTimer->start(1000);

    connect(m_manager, &AlarmManager::alarmsChanged, this, &Scheduler::nextAlarmChanged);
}

int Scheduler::nextAlarmSeconds() const
{
    return secondsUntilNextAlarm();
}

int Scheduler::secondsUntilNextAlarm() const
{
    QDateTime now = QDateTime::currentDateTime();
    QTime currentTime = now.time();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

    int bestDiff = -1;

    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;

        QTime alarmTime(a.hour, a.minute, 0);

        if (a.isSnooze) {
            QDateTime snoozeDt(now.date(), alarmTime);
            if (snoozeDt <= now)
                continue;
            int diff = static_cast<int>(now.secsTo(snoozeDt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            continue;
        }

        // Check if any day is selected
        bool hasDays = false;
        for (int d = 0; d < 7; ++d) {
            if (a.days[d]) { hasDays = true; break; }
        }

        if (!hasDays) {
            // One-shot: fire at the next occurrence of this time (today or tomorrow)
            QDateTime dt(now.date(), alarmTime);
            if (dt <= now)
                dt = dt.addDays(1);
            int diff = static_cast<int>(now.secsTo(dt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            continue;
        }

        // Regular alarm: find next occurrence by day
        for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
            int checkDay = (currentDay + dayOffset) % 7;
            if (!a.days[checkDay]) continue;

            QDateTime checkDt(now.date().addDays(dayOffset), alarmTime);
            if (checkDt <= now) continue;

            int diff = static_cast<int>(now.secsTo(checkDt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            break;
        }
    }

    return bestDiff;
}

void Scheduler::snooze(int snoozeMinutes)
{
    QDateTime now = QDateTime::currentDateTime();
    QDateTime snoozeTime = now.addSecs(snoozeMinutes * 60);

    Alarm snoozeAlarm;
    snoozeAlarm.hour = snoozeTime.time().hour();
    snoozeAlarm.minute = snoozeTime.time().minute();
    snoozeAlarm.isSnooze = true;
    snoozeAlarm.enabled = true;
    snoozeAlarm.wakeMode = "none";
    snoozeAlarm.fadeDuration = 5;
    snoozeAlarm.baseVolume = 20;
    snoozeAlarm.autoStopDuration = 60;
    snoozeAlarm.enableSound = true;
    snoozeAlarm.soundFile.clear();

    m_manager->addTransientAlarm(snoozeAlarm.toVariantMap());
}

void Scheduler::onCheckTimer()
{
    QDateTime now = QDateTime::currentDateTime();
    QTime currentTime = now.time();
    int currentHour = currentTime.hour();
    int currentMin = currentTime.minute();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

    // Emit countdown every second
    int secs = secondsUntilNextAlarm();
    emit countdownUpdated(secs);

    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;

        QTime alarmTime(a.hour, a.minute, 0);

        if (a.isSnooze) {
            QDateTime alarmDt(now.date(), alarmTime);
            if (alarmDt > now) continue;
            emit alarmTriggered(i);
            m_manager->removeAlarm(i);
            m_lastTriggeredMin = currentHour * 60 + currentMin;
            return;
        }

        // Check if alarm time matches current clock minute
        if (a.hour != currentHour || a.minute != currentMin)
            continue;

        int currentTick = currentHour * 60 + currentMin;
        if (currentTick == m_lastTriggeredMin)
            continue; // already triggered this minute

        // Check if any day is selected
        bool hasDays = false;
        for (int d = 0; d < 7; ++d) {
            if (a.days[d]) { hasDays = true; break; }
        }

        if (hasDays && !a.days[currentDay])
            continue; // not an active day

        // Alarm fires now
        emit alarmTriggered(i);
        m_lastTriggeredMin = currentTick;
        return;
    }

    emit nextAlarmChanged();
}
