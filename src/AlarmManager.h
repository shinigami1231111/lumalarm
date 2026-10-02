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
#include <QUuid>
#include <QSaveFile>

struct Alarm {
    QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    int hour = 7;
    int minute = 0;
    QVector<bool> days = {false, false, false, false, false, false, false};
    QString wakeMode = "mem";
    int autoStopDuration = 120;
    bool enableSound = true;
    bool enableCommand = false;
    QString command;
    bool enabled = true;
    QString soundFile;
    QString name;

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
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE void updateAlarm(int index, const QVariantMap &alarm);
    Q_INVOKABLE void loadFromFile();
    Q_INVOKABLE void saveToFile();

    QString alarmsFilePath() const;

signals:
    void alarmsChanged();
    void persistenceError(const QString &message);

private:
    QVector<Alarm> m_alarms;
    QString m_filePath;
};

#endif // ALARMMANAGER_H
