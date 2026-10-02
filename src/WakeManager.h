#ifndef WAKEMANAGER_H
#define WAKEMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>

// Result of RTC capability inspection. Distinguishes the four levels that
// must NOT be conflated: RTC exists / wakealarm interface exists / rtcwake
// tool exists / firmware will actually wake from the requested power state.
struct RtcCapabilities {
    bool available = false;            // some RTC device exists
    bool wakeAlarmSupported = false;   // /sys/class/rtc/rtcN/wakealarm exists
    bool poweroffWakeSupported = false;// rtcwake present; S5 wake itself is
                                       // firmware-dependent and is verified by
                                       // programming + reading back the alarm
    QString device;                    // e.g. "/dev/rtc0" (empty = let rtcwake autodetect)
    QString sysfsPath;                 // e.g. "/sys/class/rtc/rtc0"
    QString error;                     // why capability is missing, if any
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

    // Legacy suspend flow (rtcwake -m <mode> -s <seconds>), kept for
    // compatibility. The Scheduler's armWake() uses the verified absolute-
    // timestamp flow below instead.
    Q_INVOKABLE void prepareWake(int secondsUntilAlarm, const QString &mode = "mem");
    Q_INVOKABLE bool isRtcWakeAvailable() const;

    // ── Verified RTC wake flow (used by Scheduler) ──────────────────────────

    // Detect RTC devices and wake-alarm support through /sys/class/rtc.
    RtcCapabilities inspectRtc() const;

    // Program the RTC wake alarm for an ABSOLUTE UTC epoch timestamp using
    // `rtcwake -m no -t <epoch>` (sets the alarm without suspending). Runs
    // synchronously, captures output, checks the exit code. Refuses to claim
    // success unless the process actually succeeded.
    bool programWakeAlarm(qint64 epochUtc, QString *error = nullptr);

    // Read back /sys/class/rtc/rtcN/wakealarm and require it to match the
    // expected absolute timestamp within WakePolicy::RTC_ALARM_TOLERANCE.
    // Returns false when the interface cannot be read (explicit limitation).
    bool verifyWakeAlarm(qint64 expectedUtc, QString *error = nullptr) const;

    // Best-effort removal of a stale RTC alarm (writes 0).
    bool clearWakeAlarm(QString *error = nullptr);

    // Current value of the RTC wakealarm sysfs file in epoch seconds,
    // or -1 when unavailable.
    qint64 readWakeAlarmValue() const;

    // Request machine power-off through systemd/logind (polkit, no sudo
    // needed in a normal desktop session) with a sudo fallback. Returns the
    // actual outcome; never claims success without exit code 0.
    bool requestPowerOff(QString *error = nullptr);

signals:
    void wakePrepared();
    void wakeError(const QString &message);
    void wakeStateChanged();

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    static bool runSynchronous(const QString &program, const QStringList &args,
                               int timeoutMs, QString *error, QString *output = nullptr);

    QProcess *m_process;
    QString m_state;   // "idle", "arming", "armed", "failed"
    QString m_lastError;
};

#endif // WAKEMANAGER_H