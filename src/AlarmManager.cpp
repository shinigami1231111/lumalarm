#include "AlarmManager.h"

QJsonObject Alarm::toJson() const
{
    QJsonObject obj;
    obj["id"] = id;
    obj["hour"] = hour;
    obj["minute"] = minute;

    QJsonArray daysArr;
    for (bool d : days) {
        daysArr.append(d);
    }
    obj["days"] = daysArr;

    obj["wakeMode"] = wakeMode;
    obj["fadeDuration"] = fadeDuration;
    obj["baseVolume"] = baseVolume;
    obj["autoStopDuration"] = autoStopDuration;
    obj["enableSound"] = enableSound;
    obj["enableCommand"] = enableCommand;
    obj["command"] = command;
    obj["enabled"] = enabled;
    obj["soundFile"] = soundFile;
    obj["isSnooze"] = isSnooze;
    obj["enableChallenge"] = enableChallenge;
    obj["challengeText"] = challengeText;
    obj["wakeUpCheckEnabled"] = wakeUpCheckEnabled;
    obj["wakeUpCheckInterval"] = wakeUpCheckInterval;

    obj["soundscape"] = soundscape;
    obj["maxSnoozes"] = maxSnoozes;
    obj["challengeMode"] = challengeMode;
    obj["mathDifficulty"] = mathDifficulty;
    obj["escalatingWake"] = escalatingWake;
    obj["escalatingTimeout"] = escalatingTimeout;
    obj["note"] = note;
    obj["snoozeInterval"] = snoozeInterval;
    obj["name"] = name;

    return obj;
}

Alarm Alarm::fromJson(const QJsonObject &obj)
{
    Alarm a;
    a.id = obj["id"].toString();
    if (a.id.isEmpty())
        a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.hour = obj["hour"].toInt(7);
    a.minute = obj["minute"].toInt(0);

    QJsonArray daysArr = obj["days"].toArray();
    a.days.resize(7);
    for (int i = 0; i < 7 && i < daysArr.size(); ++i) {
        a.days[i] = daysArr[i].toBool(false);
    }

    a.wakeMode = obj["wakeMode"].toString("mem");
    a.fadeDuration = obj["fadeDuration"].toInt(15);
    a.baseVolume = obj["baseVolume"].toInt(20);
    a.autoStopDuration = obj["autoStopDuration"].toInt(120);
    a.enableSound = obj["enableSound"].toBool(true);
    a.enableCommand = obj["enableCommand"].toBool(false);
    a.command = obj["command"].toString();
    a.enabled = obj["enabled"].toBool(true);
    a.soundFile = obj["soundFile"].toString();
    a.isSnooze = obj["isSnooze"].toBool(false);
    a.enableChallenge = obj["enableChallenge"].toBool(false);
    a.challengeText = obj["challengeText"].toString();
    a.wakeUpCheckEnabled = obj["wakeUpCheckEnabled"].toBool(false);
    a.wakeUpCheckInterval = obj["wakeUpCheckInterval"].toInt(3);

    // Phase 1 fields with backward-compat defaults
    if (obj.contains("challengeMode")) {
        a.challengeMode = obj["challengeMode"].toString("typing");
    } else {
        // Migrate from old enableChallenge bool
        a.challengeMode = a.enableChallenge ? "typing" : "none";
    }
    a.soundscape = obj["soundscape"].toString();
    a.maxSnoozes = obj["maxSnoozes"].toInt(-1);
    a.mathDifficulty = obj["mathDifficulty"].toInt(0);
    a.escalatingWake = obj["escalatingWake"].toBool(false);
    a.escalatingTimeout = obj["escalatingTimeout"].toInt(60);
    a.note = obj["note"].toString();
    a.snoozeInterval = obj["snoozeInterval"].toInt(0);
    a.name = obj["name"].toString();

    return a;
}

QVariantMap Alarm::toVariantMap() const
{
    QVariantMap map;
    map["id"] = id;
    map["hour"] = hour;
    map["minute"] = minute;

    QVariantList daysList;
    for (bool d : days) {
        daysList.append(d);
    }
    map["days"] = daysList;

    map["wakeMode"] = wakeMode;
    map["fadeDuration"] = fadeDuration;
    map["baseVolume"] = baseVolume;
    map["autoStopDuration"] = autoStopDuration;
    map["enableSound"] = enableSound;
    map["enableCommand"] = enableCommand;
    map["command"] = command;
    map["enabled"] = enabled;
    map["soundFile"] = soundFile;
    map["isSnooze"] = isSnooze;
    map["enableChallenge"] = enableChallenge;
    map["challengeText"] = challengeText;
    map["wakeUpCheckEnabled"] = wakeUpCheckEnabled;
    map["wakeUpCheckInterval"] = wakeUpCheckInterval;

    map["soundscape"] = soundscape;
    map["maxSnoozes"] = maxSnoozes;
    map["challengeMode"] = challengeMode;
    map["mathDifficulty"] = mathDifficulty;
    map["escalatingWake"] = escalatingWake;
    map["escalatingTimeout"] = escalatingTimeout;
    map["note"] = note;
    map["snoozeInterval"] = snoozeInterval;
    map["name"] = name;

    return map;
}

Alarm Alarm::fromVariantMap(const QVariantMap &map)
{
    Alarm a;
    a.id = map.value("id").toString();
    if (a.id.isEmpty())
        a.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    a.hour = map.value("hour", 7).toInt();
    a.minute = map.value("minute", 0).toInt();

    QVariantList daysList = map.value("days").toList();
    a.days.resize(7);
    for (int i = 0; i < 7 && i < daysList.size(); ++i) {
        a.days[i] = daysList[i].toBool();
    }

    a.wakeMode = map.value("wakeMode", "mem").toString();
    a.fadeDuration = map.value("fadeDuration", 15).toInt();
    a.baseVolume = map.value("baseVolume", 20).toInt();
    a.autoStopDuration = map.value("autoStopDuration", 120).toInt();
    a.enableSound = map.value("enableSound", true).toBool();
    a.enableCommand = map.value("enableCommand", false).toBool();
    a.command = map.value("command").toString();
    a.enabled = map.value("enabled", true).toBool();
    a.soundFile = map.value("soundFile").toString();
    a.isSnooze = map.value("isSnooze", false).toBool();
    a.enableChallenge = map.value("enableChallenge", false).toBool();
    a.challengeText = map.value("challengeText").toString();
    a.wakeUpCheckEnabled = map.value("wakeUpCheckEnabled", false).toBool();
    a.wakeUpCheckInterval = map.value("wakeUpCheckInterval", 3).toInt();

    if (map.contains("challengeMode")) {
        a.challengeMode = map.value("challengeMode", "typing").toString();
    } else {
        a.challengeMode = a.enableChallenge ? "typing" : "none";
    }
    a.soundscape = map.value("soundscape").toString();
    a.maxSnoozes = map.value("maxSnoozes", -1).toInt();
    a.mathDifficulty = map.value("mathDifficulty", 0).toInt();
    a.escalatingWake = map.value("escalatingWake", false).toBool();
    a.escalatingTimeout = map.value("escalatingTimeout", 60).toInt();
    a.note = map.value("note").toString();
    a.snoozeInterval = map.value("snoozeInterval", 0).toInt();
    a.name = map.value("name").toString();

    return a;
}

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QDir().mkpath(configDir);
    m_filePath = configDir + "/alarms.json";
    loadFromFile();
}

QVariantList AlarmManager::alarms() const
{
    QVariantList list;
    for (const Alarm &a : m_alarms) {
        list.append(a.toVariantMap());
    }
    return list;
}

QVector<Alarm> AlarmManager::alarmList() const
{
    return m_alarms;
}

void AlarmManager::setAlarms(const QVector<Alarm> &alarms)
{
    m_alarms = alarms;
    saveToFile();
    emit alarmsChanged();
}

void AlarmManager::addAlarm(const QVariantMap &alarm)
{
    m_alarms.append(Alarm::fromVariantMap(alarm));
    saveToFile();
    emit alarmsChanged();
}

int AlarmManager::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_alarms.size(); ++i) {
        if (m_alarms[i].id == id)
            return i;
    }
    return -1;
}

void AlarmManager::removeAlarm(int index)
{
    if (index >= 0 && index < m_alarms.size()) {
        m_alarms.removeAt(index);
        m_snoozeCounts.remove(index);
        saveToFile();
        emit alarmsChanged();
    }
}

void AlarmManager::updateAlarm(int index, const QVariantMap &alarm)
{
    if (index >= 0 && index < m_alarms.size()) {
        m_alarms[index] = Alarm::fromVariantMap(alarm);
        m_snoozeCounts.remove(index);
        saveToFile();
        emit alarmsChanged();
    }
}

void AlarmManager::loadFromFile()
{
    QFile file(m_filePath);
    if (!file.exists()) {
        m_alarms.clear();
        saveToFile();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        m_alarms.clear();
        return;
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        return;
    }

    m_alarms.clear();
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        m_alarms.append(Alarm::fromJson(val.toObject()));
    }
}

void AlarmManager::saveToFile()
{
    QFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return;
    }

    QJsonArray arr;
    for (const Alarm &a : m_alarms) {
        if (a.isSnooze) continue;
        arr.append(a.toJson());
    }

    QJsonDocument doc(arr);
    file.write(doc.toJson(QJsonDocument::Indented));
    file.close();
}

QString AlarmManager::alarmsFilePath() const
{
    return m_filePath;
}

int AlarmManager::snoozeCount(int alarmIndex) const
{
    return m_snoozeCounts.value(alarmIndex, 0);
}

void AlarmManager::resetSnoozeCount(int alarmIndex)
{
    m_snoozeCounts.remove(alarmIndex);
}

void AlarmManager::incrementSnooze(int alarmIndex)
{
    m_snoozeCounts[alarmIndex] = m_snoozeCounts.value(alarmIndex, 0) + 1;
}
