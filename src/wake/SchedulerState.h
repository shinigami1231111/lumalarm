#ifndef SCHEDULERSTATE_H
#define SCHEDULERSTATE_H

#include <QDateTime>
#include <QJsonObject>
#include <QString>
#include <QTimeZone>
#include "managers/AlarmManager.h"

// ── Wake-recovery policy ─────────────────────────────────────────────────────
// Every timing window used to classify a boot relative to a scheduled wake
// lives here, in one place, with the reasoning for each value. No scattered
// magic numbers anywhere else in the scheduler.
namespace WakePolicy {
    // Boot within this window after the scheduled time is the expected case:
    // the machine was woken by RTC, Linux spent a few tens of seconds booting,
    // and the alarm must ring. This is the "boot took 37 seconds" case.
    constexpr qint64 ON_TIME_JITTER_SECONDS = 30;

    // Beyond the jitter window but still within this window is an expected
    // wake that was delivered late (firmware/platform drift, slow resume,
    // suspend interplay). The user asked for the alarm → ring it.
    constexpr qint64 LATE_GRACE_SECONDS = 5 * 60;

    // Wakes beyond the late window but within this window are "very late":
    // still clearly left over from a real scheduled wake (e.g. poweroff+RTC
    // fired while the machine was off, boot took an hour). Ring it.
    constexpr qint64 RECOVERABLE_WINDOW_SECONDS = 24 * 3600;

    // Beyond the recoverable window the occurrence is treated as missed and
    // consumed without ringing.

    // Tolerance when comparing the RTC alarm state against the programmed
    // absolute timestamp (RTC hardware may round/clamp to whole seconds).
    constexpr qint64 RTC_ALARM_TOLERANCE_SECONDS = 5;
}

// Classification of a boot relative to a scheduled wake occurrence.
enum class WakeResult { OnTime, Late, VeryLate, Missed, Unknown };

// Classify a boot. `lateSeconds` (optional) receives how far past the
// scheduled time the current time is (>= 0). `Unknown` means the occurrence
// data is invalid or the boot happened before the scheduled time.
WakeResult classifyWake(qint64 scheduledUtc, qint64 nowUtc, qint64 *lateSeconds = nullptr);

// Human-readable classification for logs.
QString wakeResultToString(WakeResult r);

// Compute the next absolute (UTC epoch seconds) occurrence of `a` strictly
// after `now`. `now` may be in any timezone; it is normalized into `tz` first,
// so DST transitions and timezone offsets in `tz` are handled by Qt's time
// machinery (no manual offset arithmetic). If the local time does not exist
// (skipped by a DST spring-forward), the occurrence is shifted forward by one
// hour — the alarm still rings at the next real point in time.
// Returns 0 when no occurrence exists within the next 7 days.
qint64 computeNextOccurrenceUtc(const Alarm &a,
                                const QDateTime &now = QDateTime(),
                                QTimeZone tz = QTimeZone::systemTimeZone());

// Persisted pending-wake record. This is the exact occurrence that survives
// power-off and reboot, so recovery is deterministic.
struct PendingWake {
    bool valid = false;
    QString alarmId;
    QString occurrenceId;   // unique per occurrence; identity = alarmId + occurrenceId
    qint64 scheduledUtc = 0; // absolute CLOCK_REALTIME seconds since epoch
    QString state = "scheduled"; // scheduled | verified | consumed | failed

    QJsonObject toJson() const;
    void fromJson(const QJsonObject &o);
};

// Atomic persistence of PendingWake (QSaveFile + .bak, same approach and
// directory as AlarmManager). Loads synchronously at construction.
class SchedulerStateStore
{
public:
    // Empty filePath → ~/.config/lumalarm/scheduler.json (per-instance path
    // allowed for tests).
    explicit SchedulerStateStore(const QString &filePath = QString());

    const PendingWake &pending() const { return m_pending; }

    // Atomic write. Returns false (with `error` filled) on failure; the
    // previous on-disk state is left untouched.
    bool persist(const PendingWake &w, QString *error = nullptr);

    QString filePath() const { return m_filePath; }

private:
    void load();

    QString m_filePath;
    PendingWake m_pending;
};

#endif // SCHEDULERSTATE_H