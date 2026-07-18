#include "AlarmManager.h"

QJsonObject Alarm::toJson() const
{
    QJsonObject obj;
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
    return obj;
}

Alarm Alarm::fromJson(const QJsonObject &obj)
{
    Alarm a;
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
    return a;
}

QVariantMap Alarm::toVariantMap() const
{
    QVariantMap map;
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
    return map;
}

Alarm Alarm::fromVariantMap(const QVariantMap &map)
{
    Alarm a;
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

void AlarmManager::addTransientAlarm(const QVariantMap &alarm)
{
    m_alarms.append(Alarm::fromVariantMap(alarm));
    // Do NOT save to file — transient alarms (snooze) are lost on restart
    emit alarmsChanged();
}

void AlarmManager::removeAlarm(int index)
{
    if (index >= 0 && index < m_alarms.size()) {
        m_alarms.removeAt(index);
        saveToFile();
        emit alarmsChanged();
    }
}

void AlarmManager::updateAlarm(int index, const QVariantMap &alarm)
{
    if (index >= 0 && index < m_alarms.size()) {
        m_alarms[index] = Alarm::fromVariantMap(alarm);
        saveToFile();
        emit alarmsChanged();
    }
}

void AlarmManager::loadFromFile()
{
    QFile file(m_filePath);
    if (!file.exists()) {
        // Start with empty alarm list — no defaults
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
        // Don't persist transient (snooze) alarms
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
