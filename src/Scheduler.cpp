#include "Scheduler.h"
#include <QDebug>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QStandardPaths>
#include <QDir>

Scheduler::Scheduler(AlarmManager *manager, QObject *parent)
    : QObject(parent)
    , m_manager(manager)
    , m_checkTimer(new QTimer(this))
{
    connect(m_checkTimer, &QTimer::timeout, this, &Scheduler::onCheckTimer);
    m_checkTimer->start(1000);

    connect(m_manager, &AlarmManager::alarmsChanged, this, &Scheduler::nextAlarmChanged);

    loadSnoozes();
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

void Scheduler::startSnoozeTimer(const QString &alarmId, int snoozeMinutes)
{
    if (alarmId.isEmpty() || snoozeMinutes <= 0) return;

    QDateTime fireAt = QDateTime::currentDateTime().addSecs(snoozeMinutes * 60);
    armSnooze(alarmId, fireAt, true);
}

void Scheduler::cancelSnooze(const QString &alarmId)
{
    auto it = m_snoozes.find(alarmId);
    if (it != m_snoozes.end()) {
        if (it->timer) {
            it->timer->stop();
            it->timer->deleteLater();
        }
        m_snoozes.erase(it);
        saveSnoozes();
    }
}

void Scheduler::armSnooze(const QString &alarmId, const QDateTime &fireAt, bool persist)
{
    // Remove any existing snooze for this alarm first.
    cancelSnooze(alarmId);

    SnoozeEntry entry;
    entry.alarmId = alarmId;
    entry.fireAt = fireAt;
    entry.timer = new QTimer(this);
    entry.timer->setSingleShot(true);
    connect(entry.timer, &QTimer::timeout, this, [this, alarmId]() {
        fireSnooze(alarmId);
    });

    qint64 ms = QDateTime::currentDateTime().msecsTo(fireAt);
    if (ms < 0) ms = 0;
    entry.timer->start(static_cast<int>(ms));

    m_snoozes.insert(alarmId, entry);
    if (persist) saveSnoozes();
}

void Scheduler::fireSnooze(const QString &alarmId)
{
    m_snoozes.remove(alarmId);
    saveSnoozes();

    int index = m_manager->indexOfId(alarmId);
    if (index < 0) return;                       // alarm removed while snoozing
    const auto alarms = m_manager->alarmList();
    if (!alarms[index].enabled) return;          // alarm disabled while snoozing

    emit alarmTriggered(index);
}

void Scheduler::restoreSnoozes()
{
    QDateTime now = QDateTime::currentDateTime();
    // loadSnoozes() already populated m_snoozes with persisted fire times but
    // no timers yet; arm each one (skip if its fire time already passed).
    QHash<QString, SnoozeEntry> persisted = m_snoozes;
    m_snoozes.clear();
    for (auto it = persisted.begin(); it != persisted.end(); ++it) {
        if (it->fireAt <= now) {
            // Snooze window already elapsed while we were closed: ring now.
            fireSnooze(it->alarmId);
        } else {
            armSnooze(it->alarmId, it->fireAt, false);
        }
    }
}

void Scheduler::onCheckTimer()
{
    QDateTime now = QDateTime::currentDateTime();
    QTime currentTime = now.time();
    int currentHour = currentTime.hour();
    int currentMin = currentTime.minute();
    int currentDay = now.date().dayOfWeek() - 1;
    if (currentDay < 0) currentDay = 6;

    // Detect system resume from suspend (timer gap > 3s)
    qint64 gap = m_lastCheckTime.msecsTo(now);
    if (gap > 3000 && m_lastCheckTime.isValid()) {
        emit systemResumed();
    }
    m_lastCheckTime = now;

    int secs = secondsUntilNextAlarm();
    emit countdownUpdated(secs);

    const QVector<Alarm> alarms = m_manager->alarmList();
    for (int i = 0; i < alarms.size(); ++i) {
        const Alarm &a = alarms[i];
        if (!a.enabled) continue;

        QTime alarmTime(a.hour, a.minute, 0);

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

            if (!hasDays) {
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

QString Scheduler::snoozeFilePath() const
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QDir().mkpath(configDir);
    return configDir + "/snoozes.json";
}

void Scheduler::saveSnoozes()
{
    QString path = snoozeFilePath();
    if (m_snoozes.isEmpty()) {
        QFile::remove(path);
        return;
    }

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonArray arr;
    for (auto it = m_snoozes.begin(); it != m_snoozes.end(); ++it) {
        QJsonObject obj;
        obj["alarmId"] = it->alarmId;
        obj["fireAt"] = it->fireAt.toString(Qt::ISODate);
        arr.append(obj);
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

void Scheduler::loadSnoozes()
{
    m_snoozes.clear();
    QFile file(snoozeFilePath());
    if (!file.exists())
        return;
    if (!file.open(QIODevice::ReadOnly))
        return;

    QByteArray data = file.readAll();
    file.close();
    if (data.trimmed().isEmpty())
        return;

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray())
        return;

    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        QJsonObject obj = val.toObject();
        QString id = obj["alarmId"].toString();
        QDateTime fireAt = QDateTime::fromString(obj["fireAt"].toString(), Qt::ISODate);
        if (id.isEmpty() || !fireAt.isValid())
            continue;
        SnoozeEntry entry;
        entry.alarmId = id;
        entry.fireAt = fireAt;
        entry.timer = nullptr;   // armed later by restoreSnoozes()
        m_snoozes.insert(id, entry);
    }
}
