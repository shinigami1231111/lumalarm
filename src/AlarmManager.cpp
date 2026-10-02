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
    obj["autoStopDuration"] = autoStopDuration;
    obj["enableSound"] = enableSound;
    obj["enableCommand"] = enableCommand;
    obj["command"] = command;
    obj["enabled"] = enabled;
    obj["soundFile"] = soundFile;
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
    a.autoStopDuration = obj["autoStopDuration"].toInt(120);
    a.enableSound = obj["enableSound"].toBool(true);
    a.enableCommand = obj["enableCommand"].toBool(false);
    a.command = obj["command"].toString();
    a.enabled = obj["enabled"].toBool(true);
    a.soundFile = obj["soundFile"].toString();
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
    map["autoStopDuration"] = autoStopDuration;
    map["enableSound"] = enableSound;
    map["enableCommand"] = enableCommand;
    map["command"] = command;
    map["enabled"] = enabled;
    map["soundFile"] = soundFile;
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
    a.autoStopDuration = map.value("autoStopDuration", 120).toInt();
    a.enableSound = map.value("enableSound", true).toBool();
    a.enableCommand = map.value("enableCommand", false).toBool();
    a.command = map.value("command").toString();
    a.enabled = map.value("enabled", true).toBool();
    a.soundFile = map.value("soundFile").toString();
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
        m_alarms.clear();
        saveToFile();
        return;
    }

    if (!file.open(QIODevice::ReadOnly)) {
        emit persistenceError("Cannot open alarms file: " + file.errorString());
        return;
    }

    QByteArray data = file.readAll();
    file.close();

    if (data.trimmed().isEmpty()) {
        QString backupPath = m_filePath + ".bak";
        if (QFile::exists(backupPath)) {
            emit persistenceError("Alarms file empty, loading backup");
            QFile bak(backupPath);
            if (bak.open(QIODevice::ReadOnly)) {
                data = bak.readAll();
                bak.close();
            }
        }
        if (data.trimmed().isEmpty()) {
            m_alarms.clear();
            return;
        }
    }

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isArray()) {
        QString backupPath = m_filePath + ".bak";
        if (QFile::exists(backupPath)) {
            emit persistenceError("Alarms file corrupted, loading backup");
            QFile bak(backupPath);
            if (bak.open(QIODevice::ReadOnly)) {
                QByteArray bakData = bak.readAll();
                bak.close();
                doc = QJsonDocument::fromJson(bakData);
            }
        }
        if (!doc.isArray()) {
            emit persistenceError("Alarms file corrupted and no valid backup found");
            return;
        }
    }

    m_alarms.clear();
    QJsonArray arr = doc.array();
    for (const QJsonValue &val : arr) {
        m_alarms.append(Alarm::fromJson(val.toObject()));
    }
}

void AlarmManager::saveToFile()
{
    QJsonArray arr;
    for (const Alarm &a : m_alarms) {
        arr.append(a.toJson());
    }

    QJsonDocument doc(arr);
    QByteArray data = doc.toJson(QJsonDocument::Indented);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        emit persistenceError("Cannot write alarms file: " + file.errorString());
        return;
    }

    QString backupPath = m_filePath + ".bak";
    if (QFile::exists(m_filePath)) {
        QFile::remove(backupPath);
        QFile::copy(m_filePath, backupPath);
    }

    file.write(data);
    if (!file.commit()) {
        emit persistenceError("Failed to commit alarms file: " + file.errorString());
    }
}

QString AlarmManager::alarmsFilePath() const
{
    return m_filePath;
}
