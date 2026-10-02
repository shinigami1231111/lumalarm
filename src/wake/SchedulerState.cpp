#include "wake/SchedulerState.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QTimeZone>
#include <QUuid>

WakeResult classifyWake(qint64 scheduledUtc, qint64 nowUtc, qint64 *lateSeconds)
{
    if (lateSeconds)
        *lateSeconds = qMax<qint64>(0, nowUtc - scheduledUtc);

    if (scheduledUtc <= 0)
        return WakeResult::Unknown;

    const qint64 late = nowUtc - scheduledUtc;

    if (late < 0) {
        // Booted before the scheduled time: occurrence is still in the
        // future. Nothing to classify yet.
        return WakeResult::Unknown;
    }
    if (late <= WakePolicy::ON_TIME_JITTER_SECONDS)
        return WakeResult::OnTime;
    if (late <= WakePolicy::LATE_GRACE_SECONDS)
        return WakeResult::Late;
    if (late <= WakePolicy::RECOVERABLE_WINDOW_SECONDS)
        return WakeResult::VeryLate;
    return WakeResult::Missed;
}

QString wakeResultToString(WakeResult r)
{
    switch (r) {
    case WakeResult::OnTime:   return "OnTime";
    case WakeResult::Late:     return "Late";
    case WakeResult::VeryLate: return "VeryLate";
    case WakeResult::Missed:   return "Missed";
    case WakeResult::Unknown:  return "Unknown";
    }
    return "Unknown";
}

qint64 computeNextOccurrenceUtc(const Alarm &a, const QDateTime &now, QTimeZone tz)
{
    const QDateTime base = now.isValid() ? now : QDateTime::currentDateTime();
    const QDateTime n = base.toTimeZone(tz);
    const QTime alarmTime(a.hour, a.minute, 0);

    // Local-time occurrence → UTC epoch. If the wall-clock time does not exist
    // (skipped by a DST spring-forward) Qt normally maps it to the next real
    // instant; on builds where it reports invalid instead, nudge forward by
    // one hour at a time until a real instant is found.
    auto occurrenceUtcAt = [&](const QDate &d) -> qint64 {
        QTime t = alarmTime;
        for (int nudge = 0; nudge < 48; ++nudge) {
            QDateTime dt(d, t, tz);
            if (dt.isValid())
                return dt.toUTC().toSecsSinceEpoch();
            t = t.addSecs(3600);
        }
        return 0;
    };

    const qint64 baseEpoch = n.toSecsSinceEpoch();

    bool hasDays = false;
    for (int d = 0; d < 7; ++d) {
        if (a.days[d]) { hasDays = true; break; }
    }

    if (!hasDays) {
        qint64 occ = occurrenceUtcAt(n.date());
        if (occ <= baseEpoch)
            occ = occurrenceUtcAt(n.date().addDays(1));
        return occ;
    }

    const int currentDay = n.date().dayOfWeek() - 1; // Monday = 0
    for (int dayOffset = 0; dayOffset < 7; ++dayOffset) {
        const int checkDay = (currentDay + dayOffset) % 7;
        if (!a.days[checkDay]) continue;
        const qint64 occ = occurrenceUtcAt(n.date().addDays(dayOffset));
        if (occ <= baseEpoch) continue;
        return occ;
    }

    return 0;
}

QJsonObject PendingWake::toJson() const
{
    QJsonObject o;
    o["alarmId"] = alarmId;
    o["occurrenceId"] = occurrenceId;
    o["scheduledUtc"] = scheduledUtc;
    o["state"] = state;
    return o;
}

void PendingWake::fromJson(const QJsonObject &o)
{
    alarmId = o["alarmId"].toString();
    occurrenceId = o["occurrenceId"].toString();
    scheduledUtc = static_cast<qint64>(o["scheduledUtc"].toDouble());
    state = o["state"].toString("scheduled");
    valid = !alarmId.isEmpty() && scheduledUtc > 0;
}

SchedulerStateStore::SchedulerStateStore(const QString &filePath)
    : m_filePath(filePath)
{
    if (m_filePath.isEmpty()) {
        const QString configDir =
            QStandardPaths::writableLocation(QStandardPaths::ConfigLocation) + "/lumalarm";
        QDir().mkpath(configDir);
        m_filePath = configDir + "/scheduler.json";
    }
    load();
}

void SchedulerStateStore::load()
{
    m_pending = PendingWake();
    QFile f(m_filePath);
    if (!f.exists())
        return;
    if (!f.open(QIODevice::ReadOnly)) {
        qWarning() << "[SchedulerState] ERROR: cannot open" << m_filePath << f.errorString();
        return;
    }
    const QByteArray data = f.readAll();
    f.close();
    if (data.trimmed().isEmpty())
        return;

    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject()) {
        qWarning() << "[SchedulerState] ERROR: state file corrupted, ignoring" << m_filePath;
        return;
    }
    m_pending.fromJson(doc.object());
}

bool SchedulerStateStore::persist(const PendingWake &w, QString *error)
{
    const QJsonDocument doc(w.toJson());
    const QByteArray data = doc.toJson(QJsonDocument::Indented);

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        qWarning() << "[SchedulerState] ERROR: cannot write" << m_filePath << file.errorString();
        if (error)
            *error = "cannot open scheduler state: " + file.errorString();
        return false;
    }

    const QString backupPath = m_filePath + ".bak";
    if (QFile::exists(m_filePath)) {
        QFile::remove(backupPath);
        QFile::copy(m_filePath, backupPath);
    }

    file.write(data);
    if (!file.commit()) {
        qWarning() << "[SchedulerState] ERROR: failed to commit" << m_filePath << file.errorString();
        if (error)
            *error = "failed to commit scheduler state: " + file.errorString();
        return false;
    }

    m_pending = w;
    return true;
}