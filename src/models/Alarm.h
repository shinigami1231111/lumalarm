#ifndef ALARM_H
#define ALARM_H

#include <QJsonObject>
#include <QString>
#include <QUuid>
#include <QVariantMap>
#include <QVector>

// Plain alarm value type. Extracted from AlarmManager.h so AlarmModel.h and
// AlarmManager.h can both use it without include cycles. The on-disk JSON
// schema (toJson/fromJson) is unchanged — alarms.json written by 1.0.1 stays
// fully readable.
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

#endif // ALARM_H
