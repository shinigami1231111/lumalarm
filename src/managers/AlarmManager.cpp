#include "managers/AlarmManager.h"

// ── struct Alarm — serialization kept byte-for-byte from the pre-refactor
// ── version; this is the on-disk schema users upgrading from 1.0.1 rely on.

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

// ── AlarmManager ─────────────────────────────────────────────────────────────

namespace {
// RAII flag: model resets performed while loading must not trigger the
// changed() handler's automatic saveToFile() (the old load never wrote back,
// except when the file was missing entirely).
class SuppressPersist
{
public:
    explicit SuppressPersist(bool &flag) : m_flag(flag) { m_flag = true; }
    ~SuppressPersist() { m_flag = false; }
    SuppressPersist(const SuppressPersist &) = delete;
    SuppressPersist &operator=(const SuppressPersist &) = delete;

private:
    bool &m_flag;
};
} // namespace

AlarmManager::AlarmManager(QObject *parent)
    : QObject(parent)
    , m_model(new AlarmModel(this))
{
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
    QDir().mkpath(configDir);
    m_filePath = configDir + "/alarms.json";

    // Every model mutation persists the file and notifies via the historical
    // alarmsChanged() signal (Scheduler.cpp and main.qml listen to it).
    connect(m_model, &AlarmModel::changed, this, [this] {
        if (m_suppressPersist)
            return;
        saveToFile();
        emit alarmsChanged();
    });

    loadFromFile();
}

AlarmModel *AlarmManager::model() const
{
    return m_model;
}

QVector<Alarm> AlarmManager::alarmList() const
{
    return m_model->alarms();
}

void AlarmManager::setAlarms(const QVector<Alarm> &alarms)
{
    {
        const SuppressPersist guard(m_suppressPersist);
        m_model->resetFromList(alarms);
    }
    saveToFile();
    emit alarmsChanged();
}

void AlarmManager::addAlarm(const QVariantMap &alarm)
{
    // changed() -> save + alarmsChanged(), exactly like the old
    // append + saveToFile() + emit alarmsChanged().
    m_model->addAlarm(Alarm::fromVariantMap(alarm));
}

int AlarmManager::indexOfId(const QString &id) const
{
    return m_model->indexOfId(id);
}

void AlarmManager::removeAlarm(int index)
{
    // Invalid index is a no-op, same as before.
    m_model->removeAlarm(index);
}

void AlarmManager::updateAlarm(int index, const QVariantMap &alarm)
{
    // Alarm::fromVariantMap keeps the old semantics — a map without "id"
    // (the editor and the list toggle always sent one) gets a fresh id.
    m_model->updateAlarm(index, Alarm::fromVariantMap(alarm));
}

void AlarmManager::setAlarmEnabled(const QString &id, bool enabled)
{
    // Resolve by id, not by index: AlarmModel::setEnabled re-sorts
    // (enabled-first), so an index captured before the toggle could already
    // be stale for the next operation. Mutating in place (via setEnabled)
    // preserves the id — updateAlarm() with a map lacking "id" would mint a
    // fresh UUID, breaking root.selectedId in AlarmListView and Scheduler's
    // PendingWake.alarmId.
    const int row = m_model->indexOfId(id);
    if (row < 0)
        return;   // unknown id — no-op, same guard style as removeAlarm()
    m_model->setEnabled(row, enabled);
}

void AlarmManager::loadFromFile()
{
    const SuppressPersist guard(m_suppressPersist);

    QFile file(m_filePath);
    if (!file.exists()) {
        m_model->resetFromList({});
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
            m_model->resetFromList({});
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

    QVector<Alarm> alarms;
    const QJsonArray arr = doc.array();
    alarms.reserve(arr.size());
    for (const QJsonValue &val : arr) {
        alarms.append(Alarm::fromJson(val.toObject()));
    }
    m_model->resetFromList(alarms);
}

void AlarmManager::saveToFile()
{
    const QVector<Alarm> alarms = m_model->alarms();

    QJsonArray arr;
    for (const Alarm &a : alarms) {
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
