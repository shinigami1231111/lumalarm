#include "Scheduler.h"
#include <QDebug>
#include <cmath>

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

        bool hasDays = false;
        for (int d = 0; d < 7; ++d) {
            if (a.days[d]) { hasDays = true; break; }
        }

        if (!hasDays) {
            QDateTime dt(now.date(), alarmTime);
            if (dt <= now)
                dt = dt.addDays(1);
            int diff = static_cast<int>(now.secsTo(dt));
            if (bestDiff < 0 || diff < bestDiff)
                bestDiff = diff;
            continue;
        }

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
    int currentSec = currentTime.second();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

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

        // Check if alarm time matches current clock
        if (a.hour != currentHour || a.minute != currentMin)
            goto check_soundscape;

        {
            int currentTick = currentHour * 60 + currentMin;
            if (currentTick == m_lastTriggeredMin)
                goto check_soundscape;

            bool hasDays = false;
            for (int d = 0; d < 7; ++d) {
                if (a.days[d]) { hasDays = true; break; }
            }

            if (hasDays && !a.days[currentDay])
                goto check_soundscape;

            // Alarm fires now
            m_soundscapeFired.remove(i);
            emit alarmTriggered(i);
            m_lastTriggeredMin = currentTick;
            return;
        }

check_soundscape:
        // Check if soundscape should start (~90s before alarm)
        if (!a.soundscape.isEmpty() && !m_soundscapeFired.contains(i)) {
            int secondsToAlarm = 0;
            QDateTime alarmDt(now.date(), alarmTime);

            bool hasDays = false;
            for (int d = 0; d < 7; ++d) {
                if (a.days[d]) { hasDays = true; break; }
            }

            if (a.isSnooze) {
                if (alarmDt <= now) continue;
                secondsToAlarm = static_cast<int>(now.secsTo(alarmDt));
            } else if (!hasDays) {
                if (alarmDt <= now) alarmDt = alarmDt.addDays(1);
                secondsToAlarm = static_cast<int>(now.secsTo(alarmDt));
            } else {
                for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
                    int checkDay = (currentDay + dayOffset) % 7;
                    if (!a.days[checkDay]) continue;
                    QDateTime checkDt(now.date().addDays(dayOffset), alarmTime);
                    if (checkDt <= now) continue;
                    secondsToAlarm = static_cast<int>(now.secsTo(checkDt));
                    break;
                }
            }

            if (secondsToAlarm > 0 && secondsToAlarm <= 90) {
                m_soundscapeFired.insert(i);
                emit soundscapeStarting(i);
            }
        }
    }

    emit nextAlarmChanged();
}
