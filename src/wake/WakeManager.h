#ifndef WAKEMANAGER_H
#define WAKEMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

// Result of RTC capability inspection. Distinguishes the four levels that
// must NOT be conflated: RTC exists / wakealarm interface exists / rtcwake
// tool exists / firmware will actually wake from the requested power state.
struct RtcCapabilities {
    bool available = false;             // some RTC device exists
    bool wakeAlarmSupported = false;    // /sys/class/rtc/rtcN/wakealarm exists
    bool poweroffWakeSupported = false; // rtcwake present; S5 wake itself is
                                        // firmware-dependent and is verified by
                                        // programming + reading back the alarm
    QString device;                     // e.g. "/dev/rtc0" (empty = let rtcwake autodetect)
    QString sysfsPath;                  // e.g. "/sys/class/rtc/rtc0"
    QString error;                      // why capability is missing, if any
};

class WakeManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString wakeState READ wakeState NOTIFY wakeStateChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY wakeStateChanged)

public:
    explicit WakeManager(QObject *parent = nullptr);

    // State: "idle" | "arming" | "armed" | "failed"
    QString wakeState() const;
    QString lastError() const;

    // True while the D-Bus helper owns the currently programmed wake. Set only
    // after the helper confirmed a schedule, and released only after a
    // confirmed (or truthfully empty) cancellation — see clearWakeAlarm().
    // Exposed for diagnostics and regression tests; not a Q_INVOKABLE.
    bool helperOwnsWake() const;

    // Legacy suspend flow (rtcwake -m <mode> -s <seconds>), kept for
    // compatibility. The Scheduler's armWake() uses the verified absolute-
    // timestamp flow below instead.
    //
    // STG-1.3: both arguments are validated against WakePolicy BEFORE any
    // process is started or D-Bus call is made; an out-of-range delay or a
    // malformed mode fails with state "failed" and leaves any existing wake
    // untouched. STG-1.5: the fallback is a plain, unprivileged rtcwake —
    // there is no sudo in this path any more.
    Q_INVOKABLE void prepareWake(int secondsUntilAlarm, const QString &mode = "mem");
    Q_INVOKABLE bool isRtcWakeAvailable() const;

    // ── Verified RTC wake flow (used by Scheduler) ──────────────────────────

    // Detect RTC devices and wake-alarm support through /sys/class/rtc.
    RtcCapabilities inspectRtc() const;

    // Program the RTC wake alarm for an ABSOLUTE UTC epoch timestamp using
    // `rtcwake -m no -t <epoch>` (sets the alarm without suspending). Runs
    // synchronously, captures output, checks the exit code. Refuses to claim
    // success unless the process actually succeeded.
    //
    // STG-1.3: the implied delay (epochUtc - now) must satisfy the same
    // WakePolicy::isValidWakeDelay() boundary as the seconds-based APIs;
    // otherwise nothing is programmed and no privileged command is spawned.
    bool programWakeAlarm(qint64 epochUtc, QString *error = nullptr);

    // Read back /sys/class/rtc/rtcN/wakealarm and require it to match the
    // expected absolute timestamp within WakePolicy::RTC_ALARM_TOLERANCE.
    // Returns false when the interface cannot be read (explicit limitation).
    bool verifyWakeAlarm(qint64 expectedUtc, QString *error = nullptr) const;

    // Remove a programmed wake. For a helper-owned wake this cancels through
    // the helper (the only thing that can stop the transient timer) and keeps
    // ownership when that fails, so the next call retries instead of silently
    // stranding a live timer (F6). Returns false rather than a false success.
    bool clearWakeAlarm(QString *error = nullptr);

    // Current value of the RTC wakealarm sysfs file in epoch seconds,
    // or -1 when unavailable.
    qint64 readWakeAlarmValue() const;

    // Request machine power-off through systemd/logind only. STG-1.5 removed
    // the `sudo -n systemctl poweroff` fallback: power-off is authorised by
    // polkit's org.freedesktop.login1.* actions through the desktop session,
    // so no local privilege-escalation path is needed. Returns the actual
    // outcome; never claims success without exit code 0.
    bool requestPowerOff(QString *error = nullptr);

    // ── Test alarm (diagnostic; never touches the alarm list) ───────────────
    // Arm a real wake `seconds` from now through the same D-Bus helper path a
    // normal alarm uses, without creating or modifying any alarm, and without
    // writing alarms.json or scheduler.json. `seconds` must satisfy
    // WakePolicy::isValidWakeDelay() and be within [kTestWakeMinSeconds,
    // kTestWakeMaxSeconds]; an out-of-range request is refused
    // (testWakeFailed) rather than silently clamped. A wake armed earlier is
    // cancelled first, so arming twice leaves exactly one pending wake.
    // Returns true only when the helper confirmed the scheduling.
    Q_INVOKABLE bool armTestWake(int seconds);

    // Cancel a wake armed by armTestWake(). Best effort: a failure is logged
    // and leaves the current state untouched.
    Q_INVOKABLE void cancelTestWake();

signals:
    void wakePrepared();
    void wakeError(const QString &message);
    void wakeStateChanged();

    // Test alarm: armed with the requested duration in seconds, or the reason
    // it could not be armed (range rejection, helper unavailable, declined).
    void testWakeArmed(int seconds);
    void testWakeFailed(const QString &reason);

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

protected:
    // ── Integration seams ───────────────────────────────────────────────────
    // The only three places WakeManager talks to the outside world for wake
    // bookkeeping. They are virtual purely so the regression tests can
    // substitute the *failure* paths deterministically (precedent:
    // WizardController::armProbeWake) instead of taking down the real D-Bus
    // helper. Nothing overrides them in production.

    // Outcome of one ScheduleWake attempt against the privileged helper.
    enum class HelperResult { Scheduled, Unavailable, Declined };

    // Ask the helper to schedule a wake `seconds` from now. Unavailable
    // (ServiceUnknown / AccessDenied / transport failure) and Declined
    // (range, polkit or backend rejection) both leave the caller on the direct
    // rtcwake path. `message` receives the helper's response text.
    // Refuses outright (Declined) when `seconds` is outside the WakePolicy
    // boundary, so no out-of-range value can ever reach the privileged service.
    virtual HelperResult scheduleViaHelper(uint seconds, QString *message);

    // Ask the helper to cancel the wake it scheduled. True = helper confirmed
    // the cancellation (or truthfully reported nothing to cancel).
    virtual bool cancelViaHelper(QString *message);

    // True when systemd still reports an armed lumalarm-wake-*.timer. Used by
    // clearWakeAlarm() to tell "cancel failed, wake still alive" (report
    // failure, keep ownership) apart from "cancel failed, wake already gone".
    virtual bool helperWakeStillArmed() const;

private:
    // Accepted range for armTestWake(), in seconds. The lower bound is
    // WakePolicy::kMinWakeSeconds (60), enforced through
    // WakePolicy::isValidWakeDelay(); armTestWake additionally never exceeds
    // kTestWakeMaxSeconds, so its window is a strict subset of the boundary.
    static constexpr int kTestWakeMaxSeconds = 3600;

    static bool runSynchronous(const QString &program, const QStringList &args, int timeoutMs,
                               QString *error, QString *output = nullptr);

    // Base names of armed lumalarm-wake-*.timer units, straight from systemd.
    static QStringList armedWakeTimerBases();

    // Read the RTC wakealarm sysfs file as the current process (no privilege
    // escalation); -1 when unreadable or empty.
    static qint64 readWakeAlarmFile(const QString &sysfsPath);

    bool m_helperOwnedWake = false; // helper owns the currently programmed wake
    QProcess *m_process;
    QString m_state; // "idle", "arming", "armed", "failed"
    QString m_lastError;
};

#endif // WAKEMANAGER_H
