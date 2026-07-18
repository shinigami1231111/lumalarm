#ifndef ALARMMANAGER_H
#define ALARMMANAGER_H

#include <QObject>
#include <QVector>
#include <QVariantList>
#include <QVariantMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QStandardPaths>
#include <QHash>

struct Alarm {
    int hour = 7;
    int minute = 0;
    QVector<bool> days = {false, false, false, false, false, false, false};
    QString wakeMode = "mem";
    int fadeDuration = 15;
    int baseVolume = 20;
    int autoStopDuration = 120;
    bool enableSound = true;
    bool enableCommand = false;
    QString command;
    bool enabled = true;
    QString soundFile;
    bool isSnooze = false;
    bool enableChallenge = false;
    QString challengeText;
    bool wakeUpCheckEnabled = false;
    int wakeUpCheckInterval = 3;

    // Phase 1: Smarter Wake Experience
    QString soundscape;
    int maxSnoozes = -1;          // -1 = unlimited, 0 = no snooze
    QString challengeMode = "typing"; // "none", "typing", "math"
    int mathDifficulty = 0;       // 0=easy (add/sub), 1=hard (mul)
    bool escalatingWake = false;
    int escalatingTimeout = 60;   // seconds before forced challenge
    QString note;

    QJsonObject toJson() const;
    static Alarm fromJson(const QJsonObject &obj);
    QVariantMap toVariantMap() const;
    static Alarm fromVariantMap(const QVariantMap &map);
};

class AlarmManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantList alarms READ alarms NOTIFY alarmsChanged)

public:
    explicit AlarmManager(QObject *parent = nullptr);

    QVariantList alarms() const;
    QVector<Alarm> alarmList() const;
    void setAlarms(const QVector<Alarm> &alarms);

    Q_INVOKABLE void addAlarm(const QVariantMap &alarm);
    Q_INVOKABLE void removeAlarm(int index);
    Q_INVOKABLE void updateAlarm(int index, const QVariantMap &alarm);
    Q_INVOKABLE void loadFromFile();
    Q_INVOKABLE void saveToFile();

    // Adds a transient alarm (e.g. snooze) that won't be persisted to alarms.json
    Q_INVOKABLE void addTransientAlarm(const QVariantMap &alarm);

    // Snooze tracking (in-memory only, resets each time alarm fires fresh)
    Q_INVOKABLE int snoozeCount(int alarmIndex) const;
    Q_INVOKABLE void resetSnoozeCount(int alarmIndex);
    Q_INVOKABLE void incrementSnooze(int alarmIndex);

    QString alarmsFilePath() const;

signals:
    void alarmsChanged();
    void alarmDismissed(int alarmIndex, int stageReached);

private:
    QVector<Alarm> m_alarms;
    QHash<int, int> m_snoozeCounts;
    QString m_filePath;
};

#endif // ALARMMANAGER_H
