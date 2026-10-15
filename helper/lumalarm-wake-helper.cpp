// Lumalarm wake helper — org.lumalarm.Helper, a D-Bus *system* service.
//
// Single-purpose, root-run replacement for the old sudoers NOPASSWD rule:
// the desktop app no longer executes rtcwake with elevated privileges —
// it asks this helper over the system bus instead.
//
// Contract (data/org.lumalarm.Helper.xml):
//   ScheduleWake(u seconds, s mode) -> (b success, s message)
//   CancelWake()            -> (b success, s message)
//   GetStatus()             -> (s status)   idle | scheduled | scheduled:<n> | error
//
// Security rules kept deliberately narrow:
//   - seconds must satisfy WakePolicy::isValidWakeDelay() i.e. [60, 86400],
//     checked from the shared header src/wake/WakeSecurity.h BEFORE anything
//     else, so the app and the helper can never enforce different bounds.
//   - mode must satisfy WakePolicy::isValidWakeMode() i.e. one of
//     WakePolicy::wakeModeChoices() — the SAME list the app checks.
//   - rtcwake -m <mode> -s <N>, started DETACHED. It programs the RTC alarm
//     and enters the sleep state in one step, and it only returns once the
//     machine has resumed, so it must never run inside a synchronous D-Bus
//     slot. No systemd anywhere: Lumalarm must work with and without it.
//   - QProcess argument lists and execvp argv ONLY — no system(), no shell,
//     no string interpolation into a shell, no configuration. The single
//     exception to "no file I/O" is the wake record described below: one
//     fixed path under /run, opened with O_NOFOLLOW, written and read by
//     nobody but root.
//   - every accepted and rejected call is logged with the caller's UID.
//   - privilege gate: uid 0 passes (fast path); every other caller must
//     satisfy the polkit action org.lumalarm.schedule.wake (auth_admin_keep),
//     checked through `pkcheck --action-id … --system-bus-name <name>`
//     (or --process <pid>) WITH --allow-user-interaction so the desktop
//     polkit agent can actually run the challenge the policy describes.
//
// State (STG-1.4, option 1a — chosen by the user 2026-10-08): a wake armed by
// THIS process is tracked in memory; a wake armed before a helper RESTART is
// rediscovered from /run/lumalarm/wake, a record written on arm, read on start
// and removed on cancel. Precedence is hardware > record: `rtcwake -m show` is
// the only source that cannot lie, so a stale record is detected and dropped
// rather than believed. The file buys the reporting; the RTC still decides
// whether anything is actually armed.

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusContext>
#include <QDBusError>
#include <QDateTime>
#include <QDebug>
#include <QElapsedTimer>
#include <QProcess>
#include <QStringList>
#include <QVariantMap>
#include <QUuid>
#include <QVector>

#include <cerrno>
#include <csignal> // kill, SIGTERM
#include <cstring>

#include <fcntl.h> // pipe, fcntl, open, O_NOFOLLOW, FD_CLOEXEC
#include <sys/stat.h> // mkdir, stat, fchmod, S_ISDIR
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h> // fork, setsid, execvp, read, write, unlink

#include "wake/WakeCaps.h"
#include "wake/WakeSecurity.h"

namespace {
// Upper bound on one pkcheck run. Generous because --allow-user-interaction
// may hold the call open while a human answers the polkit dialog; bounded so
// a helper with no reachable agent cannot be wedged forever. Must stay BELOW
// the client's kHelperCallTimeoutMs (70 s) so the client never gives up first
// and double-arms on its direct-rtcwake fallback.
constexpr int kPkcheckTimeoutMs = 60000;

// How long after arming an absent RTC alarm is still treated as "not
// programmed yet" rather than "the wake fired". The detached worker needs a
// few hundred milliseconds to open /dev/rtc0 and set the alarm, and a
// GetStatus() landing inside that window must not conclude the wake is gone
// and erase its record. Generous against a loaded system; still short enough
// that a wake which really did fire is reported on the next status call.
constexpr qint64 kArmGraceMs = 10000;

// How long ScheduleWake waits for a detached arm-only worker to make the
// alarm visible before concluding the arm was lost and retrying once.
// Arming is milliseconds on an idle machine and can take seconds under load
// (same finding kArmGraceMs was sized from), while staying far below the
// client's 70 s call timeout.
constexpr int kArmVerifyTimeoutMs = 2500;

// Runs `program args` and returns its trimmed stdout.
//
// Kept separate from runCommand() because some calls need the OUTPUT, not a
// pass/fail: `rtcwake -m show` is only useful as text, and stderr has to stay
// distinguishable from stdout because "the RTC is busy" is an answer rather
// than a failure.
QString runForOutput(const QString &program, const QStringList &args, bool *ok,
                     QString *stderrText, int timeoutMs)
{
    if (ok)
        *ok = false;
    if (stderrText)
        stderrText->clear();

    QProcess p;
    p.start(program, args); // explicit argument list — never a shell
    if (!p.waitForStarted(5000)) {
        if (stderrText)
            *stderrText = QStringLiteral("failed to start: ") + p.errorString();
        return QString();
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (stderrText)
            *stderrText = QStringLiteral("timed out");
        return QString();
    }
    if (stderrText)
        *stderrText = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (ok)
        *ok = (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0);
    return QString::fromUtf8(p.readAllStandardOutput()).trimmed();
}

// Start `program` in its own session and never wait for it.
//
// rtcwake has to outlive this call: it is the process that holds the machine
// in the sleep state until the RTC alarm fires, so it cannot run under
// runCommand() (which blocks until the child exits) and ScheduleWake has to
// return long before the machine resumes. A single fork would leave a zombie
// behind on every wake, so this double-forks — the intermediate child is
// reaped right here, and the grandchild is adopted by init when it exits.
//
// The worker reports itself through a close-on-exec pipe: it writes its own
// PID first and then execs, so a successful exec leaves the pipe closed with
// only those 4 bytes (the parent's second read sees EOF) while a failed one
// writes errno after them. The intermediate child writes -1 when fork()
// fails. Writing the PID is what lets ScheduleWake stop a wake it armed:
// rtcwake keeps /dev/rtc0 open for as long as the machine sleeps, so without
// this handle a cancel or a re-arm can only fail with "Device or resource
// busy".
//
// argv is built in the parent on purpose: nothing may allocate between fork()
// and execvp(), which is only async-signal-safe in a multi-threaded process.
bool spawnDetached(const QString &program, const QStringList &args, qint64 *workerPidOut,
                   QString *error)
{
    const auto fail = [error](const QString &text) {
        if (error)
            *error = text;
        return false;
    };

    QVector<QByteArray> storage;
    storage.reserve(args.size() + 1);
    storage.append(program.toLocal8Bit());
    for (const QString &arg : args)
        storage.append(arg.toLocal8Bit());

    QVector<char *> argv;
    argv.reserve(storage.size() + 1);
    for (QByteArray &bytes : storage)
        argv.append(bytes.data());
    argv.append(nullptr);

    int fds[2];
    if (::pipe(fds) != 0)
        return fail(
            QStringLiteral("pipe: %1").arg(QString::fromLocal8Bit(::strerror(errno))));
    ::fcntl(fds[1], F_SETFD, FD_CLOEXEC);

    const pid_t pid = ::fork();
    if (pid < 0) {
        const int e = errno;
        ::close(fds[0]);
        ::close(fds[1]);
        return fail(
            QStringLiteral("fork: %1").arg(QString::fromLocal8Bit(::strerror(e))));
    }

    if (pid == 0) {
        // Intermediate child: exit at once so the parent's waitpid() reaps it
        // immediately. It speaks into the pipe ONLY when it could not make the
        // worker at all — writing the PID here as well would push a second word
        // into the stream and the parent would read that word as the worker's
        // errno, reporting a bogus failure for a spawn that actually succeeded.
        const pid_t worker = ::fork();
        if (worker == 0) {
            ::setsid();
            const pid_t self = ::getpid(); // PID first, so the parent can act on it
            const ssize_t wroteSelf = ::write(fds[1], &self, sizeof(self));
            (void)wroteSelf;
            ::execvp(argv.first(), argv.data());
            const int e = errno; // only reached when exec failed
            const ssize_t ignored = ::write(fds[1], &e, sizeof(e));
            (void)ignored;
            ::_exit(127);
        }
        if (worker < 0) {
            const int failed = -1;
            const ssize_t ignored = ::write(fds[1], &failed, sizeof(failed));
            (void)ignored;
        }
        ::close(fds[1]);
        ::_exit(0);
    }

    ::close(fds[1]);

    // First word: the worker's PID (or -1 when the intermediate could not
    // fork). Second word: only present when exec failed, because a successful
    // exec closes the pipe and leaves EOF here.
    int workerPid = -1;
    const ssize_t gotPid = ::read(fds[0], &workerPid, sizeof(workerPid));
    int execErrno = 0;
    const ssize_t gotErr = (gotPid == static_cast<ssize_t>(sizeof(workerPid)) && workerPid > 0)
                               ? ::read(fds[0], &execErrno, sizeof(execErrno))
                               : 0;
    ::close(fds[0]);

    int status = 0;
    if (::waitpid(pid, &status, 0) < 0)
        return fail(
            QStringLiteral("waitpid: %1").arg(QString::fromLocal8Bit(::strerror(errno))));

    if (gotPid != static_cast<ssize_t>(sizeof(workerPid)))
        return fail(
            QStringLiteral("read: %1").arg(QString::fromLocal8Bit(::strerror(errno))));
    if (workerPid <= 0)
        return fail(QStringLiteral("fork: %1")
                        .arg(QString::fromLocal8Bit(::strerror(EAGAIN))));
    if (gotErr == static_cast<ssize_t>(sizeof(execErrno)))
        return fail(QStringLiteral("%1: %2")
                        .arg(program, QString::fromLocal8Bit(::strerror(execErrno))));
    if (gotErr < 0)
        return fail(
            QStringLiteral("read: %1").arg(QString::fromLocal8Bit(::strerror(errno))));

    if (workerPidOut)
        *workerPidOut = workerPid;
    return true;
}

// True while `pid` is still our rtcwake.
//
// The double-fork means init reaps the worker, not this process, so there is
// no waitpid() to learn that it is gone — and a PID alone is not a safe
// signal target: once the machine has woken, the worker is gone but its number
// may already belong to something else entirely, and this helper runs as root.
// /proc/<pid>/comm is read back before any kill() for exactly that reason.
// This is a process-safety check, not application state: nothing is written,
// nothing is remembered, nothing is consulted for behaviour beyond "may I
// signal this PID".
bool isRtcwakeProcess(qint64 pid)
{
    if (pid <= 0)
        return false;
    const QByteArray path =
        QByteArrayLiteral("/proc/") + QByteArray::number(pid) + QByteArrayLiteral("/comm");
    const int fd = ::open(path.constData(), O_RDONLY);
    if (fd < 0)
        return false;
    char buf[64] = {0};
    const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0)
        return false;
    return QByteArray(buf, int(n)).trimmed() == QByteArrayLiteral("rtcwake");
}

// ── Wake record (option 1a, chosen by the user 2026-10-08) ───────────────────
// Until now the only record of an armed wake was this process's memory, so a
// restart lost it while the RTC alarm stayed live. This file is the fix: the
// ONE path this privileged binary touches.
//
//   /run is tmpfs — cleared at every boot, so the record can never outlive the
//   alarm it describes; the directory is root-owned 0755; the file is 0600 and
//   opened O_NOFOLLOW, so a planted symlink cannot redirect a root write.
//
// Hardware still outranks it: `rtcwake -m show` is the only source that cannot
// lie, so a stale record is detected and dropped rather than believed.
constexpr const char *const kWakeRecordDir = "/run/lumalarm";
constexpr const char *const kWakeRecordPath = "/run/lumalarm/wake";
} // namespace

// D4: Q_CLASSINFO required. Without it, Qt exports the interface
// as local.<class> and D-Bus clients fail to resolve it.
class WakeHelper : public QObject, protected QDBusContext
{
    Q_OBJECT
    // Export the slots under the real bus interface name instead of Qt's
    // default local.<binary>.<class> — clients (WakeManager, busctl) look
    // up org.lumalarm.Helper.ScheduleWake/CancelWake/GetStatus.
    Q_CLASSINFO("D-Bus Interface", "org.lumalarm.Helper")

public:
    explicit WakeHelper(QObject *parent = nullptr) : QObject(parent) {}

public slots:
    // `mode` is an IN arg: the rtcwake sleep state selected in Settings
    // (WakePolicy::wakeModeChoices). Out-args follow Qt's D-Bus convention
    // (verified against the qdbusxml2cpp-generated adaptor for
    // data/org.lumalarm.Helper.xml): return value = first OUT arg,
    // non-const reference = second OUT arg.
    bool ScheduleWake(uint seconds, const QString &mode, QString &message);
    bool CancelWake(QString &message);
    QString GetStatus();
    // WAKE-1 §A: what this machine can actually do, read from the kernel's
    // own files — sleep states, mem-sleep types + default, RTC list + which
    // one has wake enabled. Read-only like GetStatus, so no polkit gate.
    QVariantMap GetCapabilities();

private:
    // Caller's UID as tracked by the bus daemon; ~0u when not called via D-Bus.
    uint callerUid() const;
    // Caller's PID as tracked by the bus daemon; ~0u when unavailable.
    uint callerPid() const;
    // Polkit gate for the privileged operations. uid 0 passes directly;
    // everyone else must satisfy org.lumalarm.schedule.wake, checked with
    // `pkcheck` (QProcess argument list only — fail-closed when pkcheck is
    // unavailable or errors).
    bool authorize(uint uid, QString *reason) const;

    bool runCommand(const QString &program, const QStringList &args, QString *error,
                    int timeoutMs = 20000) const;

    // Base names (no .timer) of every armed lumalarm-wake-*.timer in systemd,
    // enumerated fresh on every call — never from this process's memory.
    QStringList armedTimerBasesFromSystemd() const;
    // Description= of a transient timer, used to recover the requested delay.
    QString timerDescription(const QString &unitBase) const;
    // Reconcile in-memory state with the RTC and the wake record. Called
    // before status reporting so a restart cannot report "idle" for a wake
    // that is still armed.
    void rediscoverState();

    // Stop `<base>.timer` (and its service/failed state). True only when the
    // timer is really gone afterwards.
    bool stopTimer(const QString &unitBase, QString *error);

    // Stop THIS instance's detached rtcwake (it holds /dev/rtc0 while the
    // machine sleeps) and then disarm the RTC. Single entry point for both
    // CancelWake and the replace-before-arm path, so a pending wake can never
    // wedge the helper with "Device or resource busy".
    bool stopRtcwake(QString *error);

    // True once an arm-only ("no") wake's alarm is visibly programmed. Polls
    // `rtcwake -m show` for up to kArmVerifyTimeoutMs: "busy" keeps the wait
    // (the detached worker is mid-arm — the same rule rtcAlarmArmed()
    // applies), only `alarm: on` succeeds, and an "off" reading only becomes
    // conclusive at the timeout — that is a worker which lost /dev/rtc0 and
    // died unarmed.
    bool waitForRtcArm();

    // Is the RTC wakeup alarm programmed right now? rtcwake(8) prints
    // `alarm: off|on <time>` for `-m show`. The hardware outranks both this
    // process's memory and the wake record — it is the only source that
    // cannot lie about whether a wake is still live.
    bool rtcAlarmArmed() const;

    // The wake record (option 1a, chosen 2026-10-08): written while arming,
    // read when rediscovering after a restart, removed when cancelling.
    // Writing is deliberately NON-FATAL — bookkeeping must never turn a
    // successful arm into an error, or a full /run becomes yet another way
    // for Lumalarm not to sleep.
    void writeWakeRecord(const QString &mode, uint seconds, qint64 pid);
    bool readWakeRecord(QString *mode, uint *seconds, qint64 *pid) const;
    bool wakeRecordExists() const;
    void removeWakeRecord();

    QString m_state = QStringLiteral("idle"); // "idle" | "scheduled" | "error"
    QString m_lastError;
    QString m_unitName; // transient timer unit base name, systemd path
    bool m_usedRtcwake = false;
    qint64 m_rtcwakePid = 0; // detached rtcwake worker, 0 when none is held
    uint m_seconds = 0; // accepted request, echoed by GetStatus
    qint64 m_armedAtMs = 0; // when this instance armed, for the grace window
};

uint WakeHelper::callerUid() const
{
    if (!calledFromDBus() || message().service().isEmpty())
        return ~0u;
    QDBusConnectionInterface *busInterface = connection().interface();
    if (!busInterface)
        return ~0u;
    const QDBusReply<uint> uid = busInterface->serviceUid(message().service());
    return uid.isValid() ? uid.value() : ~0u;
}

uint WakeHelper::callerPid() const
{
    if (!calledFromDBus() || message().service().isEmpty())
        return ~0u;
    QDBusConnectionInterface *busInterface = connection().interface();
    if (!busInterface)
        return ~0u;
    const QDBusReply<uint> pid = busInterface->servicePid(message().service());
    return pid.isValid() ? pid.value() : ~0u;
}

// ── Systemd state discovery (STG-1.4) ────────────────────────────────────────
//
// The helper deliberately keeps no file state (see the security rules above),
// so the transient timer units themselves carry everything: their names are
// prefixed lumalarm-wake- and their Description= holds the requested delay.

QStringList WakeHelper::armedTimerBasesFromSystemd() const
{
    const QStringList args{
        QStringLiteral("list-units"),
        QStringLiteral("--all"),
        QStringLiteral("--no-legend"),
        QStringLiteral("--plain"),
        QStringLiteral("--full"),
        QStringLiteral("%1*.timer").arg(QLatin1String(WakePolicy::kTimerPrefix)),
    };
    QProcess p;
    p.start(QStringLiteral("systemctl"), args); // argument list — never a shell
    if (!p.waitForStarted(3000) || !p.waitForFinished(5000))
        return QStringList();
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0)
        return QStringList();
    return WakePolicy::armedTimerBases(QString::fromUtf8(p.readAllStandardOutput()));
}

void WakeHelper::rediscoverState()
{
    // STG-1.4 (option 1a, chosen by the user 2026-10-08). The RTC is the only
    // source that cannot lie, so it outranks both this process's memory and
    // the wake record. systemd is never consulted: this backend arms nothing
    // there, and adopting a state we did not create would put a second source
    // of truth back in play.
    //
    // Each branch costs at most one rtcwake call, and the common one — idle
    // with nothing recorded — costs none.
    if (m_usedRtcwake || m_rtcwakePid > 0) {
        // This instance armed it, so the record is only its own memory written
        // down. Hardware decides whether the wake is still live.
        if (rtcAlarmArmed()) {
            m_state = QStringLiteral("scheduled");
            m_unitName.clear();
            return;
        }
        // An off alarm means the wake fired — unless the worker has not
        // programmed /dev/rtc0 yet, which is a window of milliseconds right
        // after arming. Without the grace period, a GetStatus() racing
        // ScheduleWake() would report "idle" and erase the record of a wake
        // that is about to happen.
        if (m_armedAtMs > 0
            && (QDateTime::currentMSecsSinceEpoch() - m_armedAtMs) < kArmGraceMs)
            return; // still arming; leave the scheduled state alone

        qInfo() << "[wake-helper] armed wake is gone (it fired) -> idle";
        m_usedRtcwake = false;
        m_rtcwakePid = 0;
        m_unitName.clear();
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        m_armedAtMs = 0;
        removeWakeRecord();
        return;
    }

    if (!wakeRecordExists())
        return; // nothing armed, nothing recorded

    // A record but no memory: this helper restarted after arming.
    QString recordedMode;
    uint recordedSeconds = 0;
    qint64 recordedPid = 0;
    if (!readWakeRecord(&recordedMode, &recordedSeconds, &recordedPid)) {
        qWarning() << "[wake-helper] wake record is malformed or fails validation;"
                   << "dropping it rather than believing it";
        removeWakeRecord();
        return;
    }

    if (rtcAlarmArmed()) {
        if (m_state != QLatin1String("scheduled") || m_seconds != recordedSeconds)
            qInfo() << "[wake-helper] rediscovered armed wake from its record: mode"
                    << recordedMode << "seconds" << recordedSeconds << "pid" << recordedPid;
        m_state = QStringLiteral("scheduled");
        m_seconds = recordedSeconds;
        m_lastError.clear();
        m_unitName.clear();
        // The PID is adopted but not trusted: stopRtcwake() re-checks it
        // against /proc/<pid>/comm before signalling, so a recycled PID
        // cannot be killed by a stale record.
        m_usedRtcwake = false;
        m_rtcwakePid = recordedPid;
        return;
    }

    // The record describes a wake that never happened — the helper died
    // between writing it and starting rtcwake. Drop it rather than report a
    // wake the hardware does not confirm.
    qInfo() << "[wake-helper] stale wake record (no alarm armed) -> idle";
    removeWakeRecord();
    m_state = QStringLiteral("idle");
    m_seconds = 0;
    m_lastError.clear();
    m_armedAtMs = 0;
}

bool WakeHelper::stopTimer(const QString &unitBase, QString *error)
{
    // D5: use the fully-qualified .timer name — a bare `systemctl stop
    // lumalarm-wake-<uuid>` resolves to <base>.service, "succeeds" (the
    // inactive service stops fine) and leaves the timer armed.
    if (!runCommand(QStringLiteral("systemctl"),
                    QStringList() << QStringLiteral("stop") << unitBase + QStringLiteral(".timer"),
                    error))
        return false;

    // Best-effort cleanup of the leftover service unit and any failed-unit
    // state; not fatal if they are already gone.
    runCommand(QStringLiteral("systemctl"),
               QStringList() << QStringLiteral("stop") << unitBase + QStringLiteral(".service"),
               nullptr);
    runCommand(QStringLiteral("systemctl"),
               QStringList() << QStringLiteral("reset-failed")
                             << unitBase + QStringLiteral(".timer")
                             << unitBase + QStringLiteral(".service"),
               nullptr);
    return true;
}

// Stop the detached rtcwake this instance armed, then disarm the RTC.
//
// Order matters: rtcwake holds /dev/rtc0 open for as long as it keeps the
// machine asleep, so `rtcwake -m disable` issued against a live one fails
// with "Device or resource busy" and strands the helper in "error". The
// worker is therefore signalled first and given a bounded moment to let go
// of the device before the disarm runs.
//
// Once the machine has resumed, the worker has already exited on its own and
// only the disarm is needed: isRtcwakeProcess() reports false for a dead —
// or already recycled — PID, so nothing gets signalled and an unrelated
// process can never be hit.
bool WakeHelper::stopRtcwake(QString *error)
{
    if (m_rtcwakePid > 0) {
        if (isRtcwakeProcess(m_rtcwakePid)) {
            if (::kill(static_cast<pid_t>(m_rtcwakePid), SIGTERM) != 0 && errno != ESRCH) {
                if (error)
                    *error =
                        QStringLiteral("kill: %1").arg(QString::fromLocal8Bit(::strerror(errno)));
                return false;
            }
            // Bounded at 1 s (50 * 20 ms): enough for the worker to unwind
            // and close /dev/rtc0; it is a plain process, so in practice it
            // is gone on the first or second check.
            for (int i = 0; i < 50 && isRtcwakeProcess(m_rtcwakePid); ++i)
                ::usleep(20000);
        }
        m_rtcwakePid = 0;
        m_armedAtMs = 0;
    }

    return runCommand(QStringLiteral("rtcwake"),
                      QStringList() << QStringLiteral("-m") << QStringLiteral("disable"), error);
}

bool WakeHelper::waitForRtcArm()
{
    QElapsedTimer timer;
    timer.start();
    for (;;) {
        bool ok = false;
        QString err;
        const QString out = runForOutput(QStringLiteral("rtcwake"),
                                         QStringList() << QStringLiteral("-m")
                                                       << QStringLiteral("show"),
                                         &ok, &err, 5000);
        if (ok && out.startsWith(QLatin1String("alarm: on")))
            return true;
        if (timer.elapsed() >= kArmVerifyTimeoutMs)
            return false;
        ::usleep(200000);
    }
}

// ── Wake record (option 1a, chosen 2026-10-08) ───────────────────────────────

bool WakeHelper::rtcAlarmArmed() const
{
    bool ok = false;
    QString err;
    const QString out = runForOutput(QStringLiteral("rtcwake"),
                                     QStringList() << QStringLiteral("-m")
                                                   << QStringLiteral("show"),
                                     &ok, &err, 5000);

    // `Device or resource busy` is what an armed wake looks like from the
    // outside: the detached worker holds /dev/rtc0 for as long as it needs to
    // program the alarm and keep the machine asleep. Reporting "not armed"
    // here would let a restart answer "idle" for a wake that is live, which is
    // exactly the failure STG-1.4 exists to prevent — so busy counts as armed.
    if (!ok && err.contains(QLatin1String("busy")))
        return true;

    return out.startsWith(QStringLiteral("alarm: on"));
}

bool WakeHelper::wakeRecordExists() const
{
    struct stat st;
    return ::stat(kWakeRecordPath, &st) == 0;
}

void WakeHelper::removeWakeRecord()
{
    if (::unlink(kWakeRecordPath) != 0 && errno != ENOENT)
        qWarning() << "[wake-helper] cannot remove" << kWakeRecordPath << '('
                   << ::strerror(errno) << ')';
}

void WakeHelper::writeWakeRecord(const QString &mode, uint seconds, qint64 pid)
{
    // Deliberately non-fatal: by the time this runs the alarm is already
    // armed, and refusing a working wake because /run was full would be
    // Lumalarm failing to sleep over its own bookkeeping. Every failure
    // below logs and returns — STG-1.4 reporting degrades, nothing else.
    if (::mkdir(kWakeRecordDir, 0755) != 0 && errno != EEXIST) {
        qWarning() << "[wake-helper] cannot create" << kWakeRecordDir << '('
                   << ::strerror(errno) << ") — wake armed, record not written";
        return;
    }

    struct stat st;
    if (::stat(kWakeRecordDir, &st) != 0 || !S_ISDIR(st.st_mode) || st.st_uid != 0 ||
        (st.st_mode & (S_IWGRP | S_IWOTH))) {
        qWarning() << "[wake-helper] refusing to record in" << kWakeRecordDir
                   << "— not a root-owned, non-world-writable directory";
        return;
    }

    // O_NOFOLLOW: a symlink planted at this path makes the open fail instead
    // of being written through as root. O_CLOEXEC keeps it away from the
    // detached worker. Mode 0600, re-asserted with fchmod in case a
    // pre-existing file was looser.
    const int fd =
        ::open(kWakeRecordPath, O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW | O_CLOEXEC, 0600);
    if (fd < 0) {
        qWarning() << "[wake-helper] cannot open" << kWakeRecordPath << '('
                   << ::strerror(errno) << ") — wake armed, record not written";
        return;
    }

    const QByteArray body =
        QStringLiteral("version=1\nmode=%1\nseconds=%2\npid=%3\n")
            .arg(mode)
            .arg(seconds)
            .arg(pid)
            .toUtf8();

    const ssize_t written = ::write(fd, body.constData(), size_t(body.size()));
    if (written != ssize_t(body.size()))
        qWarning() << "[wake-helper] short write to" << kWakeRecordPath << '('
                   << ::strerror(errno) << ") — wake armed, record not written";
    (void)::fchmod(fd, 0600);
    ::close(fd);
}

bool WakeHelper::readWakeRecord(QString *mode, uint *seconds, qint64 *pid) const
{
    const int fd = ::open(kWakeRecordPath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0)
        return false; // ENOENT is the normal case: no wake recorded

    char buf[512];
    const ssize_t n = ::read(fd, buf, sizeof(buf) - 1);
    ::close(fd);
    if (n <= 0)
        return false;
    buf[n] = '\0';

    // key=value per line. Unknown keys are ignored so an older helper can
    // still read a newer record, and every value that IS recognised is
    // re-validated against the SHARED allow-lists before it can influence
    // GetStatus: a file nobody checked must never decide what the bus reports.
    QString foundMode;
    qint64 foundSeconds = -1;
    qint64 foundPid = 0;

    const QStringList lines = QString::fromUtf8(buf, int(n)).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const int eq = line.indexOf(QLatin1Char('='));
        if (eq <= 0)
            continue;
        const QString key = line.left(eq).trimmed();
        const QString value = line.mid(eq + 1).trimmed();
        if (key == QLatin1String("mode")) {
            foundMode = value;
        } else if (key == QLatin1String("seconds")) {
            bool ok = false;
            const qint64 v = value.toLongLong(&ok);
            if (ok)
                foundSeconds = v;
        } else if (key == QLatin1String("pid")) {
            bool ok = false;
            const qint64 v = value.toLongLong(&ok);
            if (ok)
                foundPid = v;
        }
    }

    if (!WakePolicy::isValidWakeMode(foundMode) || !WakePolicy::isValidWakeDelay(foundSeconds))
        return false;

    if (mode)
        *mode = foundMode;
    if (seconds)
        *seconds = uint(foundSeconds);
    if (pid)
        *pid = foundPid;
    return true;
}

// D2: Polkit gate. The D-Bus policy file alone is not sufficient —
// without an explicit pkcheck here, any local process in the same
// session can call ScheduleWake without authentication.
bool WakeHelper::authorize(uint uid, QString *reason) const
{
    // Root bypasses — the helper itself runs as root and root-run clients
    // need no interactive authentication.
    if (uid == 0)
        return true;

    // Identify the caller the way polkit expects: prefer the unique bus name
    // (owned and tracked by the bus daemon, so it cannot be recycled or
    // spoofed the way a bare PID can), falling back to the bus-tracked PID
    // when the message carries no sender.
    const QString busName = calledFromDBus() ? message().service() : QString();
    const uint pid = callerPid();
    if (uid == ~0u || (busName.isEmpty() && pid == ~0u)) {
        if (reason)
            *reason = QStringLiteral("not authorized: unknown caller");
        return false;
    }

    // --allow-user-interaction is what makes the policy reachable at all:
    // data/org.lumalarm.schedule.policy declares allow_active=auth_admin_keep,
    // so without it pkcheck refuses to contact the desktop authentication
    // agent and every non-root caller is denied forever with
    // "Authorization requires authentication and -u wasn't passed".
    // The arguments (including the action id) come from the shared STG-1
    // header so they cannot drift from what the app and tests expect.
    QProcess p;
    p.start(QStringLiteral("pkcheck"), WakePolicy::pkcheckArguments(busName, pid));
    if (!p.waitForStarted(5000)) {
        if (reason)
            *reason =
                QStringLiteral("not authorized: pkcheck unavailable: %1").arg(p.errorString());
        return false;
    }
    if (!p.waitForFinished(kPkcheckTimeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (reason)
            *reason = QStringLiteral("not authorized: pkcheck timed out");
        return false;
    }
    if (p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0)
        return true;

    QString detail = QString::fromUtf8(p.readAllStandardError()).trimmed();
    if (detail.isEmpty())
        detail = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
    if (detail.isEmpty())
        detail = QStringLiteral("exit code %1").arg(p.exitCode());
    if (reason)
        *reason = QStringLiteral("not authorized (polkit): %1").arg(detail);
    return false;
}

bool WakeHelper::runCommand(const QString &program, const QStringList &args, QString *error,
                            int timeoutMs) const
{
    QProcess p;
    p.start(program, args); // explicit argument list — never a shell
    if (!p.waitForStarted(5000)) {
        if (error)
            *error = QStringLiteral("failed to start %1: %2").arg(program, p.errorString());
        return false;
    }
    if (!p.waitForFinished(timeoutMs)) {
        p.kill();
        p.waitForFinished(2000);
        if (error)
            *error = program + QStringLiteral(" timed out");
        return false;
    }
    if (p.exitStatus() != QProcess::NormalExit || p.exitCode() != 0) {
        QString errText = QString::fromUtf8(p.readAllStandardError()).trimmed();
        if (errText.isEmpty())
            errText = QString::fromUtf8(p.readAllStandardOutput()).trimmed();
        if (errText.isEmpty())
            errText = QStringLiteral("exit code %1").arg(p.exitCode());
        if (error)
            *error = QStringLiteral("%1: %2").arg(program, errText);
        return false;
    }
    return true;
}

bool WakeHelper::ScheduleWake(uint seconds, const QString &mode, QString &message)
{
    const uint uid = callerUid();

    // 1. Range gate — the first input the caller controls, checked through the
    //    same shared function the app validates with (STG-1.3).
    if (!WakePolicy::isValidWakeDelay(seconds)) {
        message = QStringLiteral("out of range");
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                   << "-> REJECTED:" << message;
        return false;
    }

    // 1b. Mode gate — the second caller-controlled input, checked before any
    //     process is started and against the same allow-list (STG-1.3), so an
    //     off-list value can never reach `rtcwake -m`.
    if (!WakePolicy::isValidWakeMode(mode)) {
        message = QStringLiteral("invalid rtcwake mode");
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "mode" << mode
                   << "-> REJECTED:" << message;
        return false;
    }

    // 2. Privilege gate — non-root callers must satisfy the polkit action
    //    org.lumalarm.schedule.wake (auth_admin_keep); uid 0 passes.
    QString denyReason;
    if (!authorize(uid, &denyReason)) {
        message = denyReason;
        qWarning() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds
                   << "-> REJECTED:" << message;
        return false;
    }

    // 3. Program the RTC alarm and enter the sleep state in ONE detached
    //    rtcwake call. rtcwake does both, and it does not return until the
    //    machine has resumed — so it can never run synchronously here (the
    //    D-Bus call must reply at once) and it must not be a child we wait
    //    on (that would strand a zombie on every wake).
    //
    //    systemd is deliberately not involved: Lumalarm has to work with and
    //    without it, and rtcwake is the one backend it uses (user, 2026-10-08).
    //    The GUI never runs `sudo` — this helper is already root.
    //
    //    Replace semantics for the sleep state: at most one Lumalarm wake
    //    exists, so drop whatever this instance still holds before arming.
    if (m_usedRtcwake || m_rtcwakePid > 0) {
        QString disarmErr;
        if (!stopRtcwake(&disarmErr)) {
            m_state = QStringLiteral("error");
            m_lastError = disarmErr;
            message = disarmErr;
            qWarning() << "[wake-helper] ScheduleWake: uid" << uid
                       << "-> REJECTED (could not disarm the previous wake):" << disarmErr;
            return false;
        }
        m_usedRtcwake = false;
        m_state = QStringLiteral("idle");
        m_seconds = 0;
    }

    // The record is written BEFORE the spawn on purpose: if the helper dies
    // between the two, the file describes a wake that was never armed — and
    // that case heals itself, because rediscoverState() asks the hardware and
    // drops any record the RTC does not confirm. Writing after the spawn would
    // leave the opposite, unfixable gap: a live alarm nobody can attribute.
    writeWakeRecord(mode, seconds, 0);

    QString err;
    qint64 workerPid = 0;
    const QStringList armArgs{QStringLiteral("-m"), mode, QStringLiteral("-s"),
                              QString::number(seconds)};
    if (spawnDetached(QStringLiteral("rtcwake"), armArgs, &workerPid, &err)) {
        writeWakeRecord(mode, seconds, workerPid); // re-written with the real PID
        m_state = QStringLiteral("scheduled");
        m_unitName.clear();
        m_usedRtcwake = true;
        m_rtcwakePid = workerPid;
        m_seconds = seconds;
        m_armedAtMs = QDateTime::currentMSecsSinceEpoch();
        m_lastError.clear();
        message = QStringLiteral("scheduled");
        qInfo() << "[wake-helper] ScheduleWake: uid" << uid << "seconds" << seconds << "mode"
                << mode << "-> accepted (rtcwake detached, pid" << workerPid << ")";

        // "Accepted" must mean the alarm really landed, not merely that the
        // worker started: on a loaded machine the worker this call is
        // replacing can still hold /dev/rtc0, the new one dies unarmed, and
        // the record above would become the lying state STG-1.4 exists to
        // prevent. Only arm-only mode is verified here — a suspending worker
        // returns only after the machine has resumed, so polling it would
        // race the sleep itself. The loser of that race is transient, so one
        // retry, then an honest error.
        if (mode == QLatin1String("no")) {
            if (waitForRtcArm())
                return true;
            qWarning() << "[wake-helper] ScheduleWake: uid" << uid
                       << "- first arm did not land, retrying once";
            (void)runCommand(QStringLiteral("rtcwake"),
                            QStringList() << QStringLiteral("-m") << QStringLiteral("disable"),
                            nullptr);
            if (spawnDetached(QStringLiteral("rtcwake"), armArgs, &workerPid, &err)) {
                writeWakeRecord(mode, seconds, workerPid);
                m_rtcwakePid = workerPid;
                m_armedAtMs = QDateTime::currentMSecsSinceEpoch();
                if (waitForRtcArm())
                    return true;
            }
            stopRtcwake(nullptr); // best-effort: drop the worker, then disarm
            removeWakeRecord();
            m_state = QStringLiteral("error");
            m_lastError = QStringLiteral("rtc alarm did not arm");
            m_usedRtcwake = false;
            m_seconds = 0;
            message = m_lastError;
            qWarning() << "[wake-helper] ScheduleWake: uid" << uid
                       << "-> REJECTED (rtc alarm did not arm)";
            return false;
        }
        return true;
    }

    // Nothing was armed, so nothing may be recorded claiming otherwise.
    removeWakeRecord();

    m_state = QStringLiteral("error");
    m_lastError = err;
    message = err;
    qWarning() << "[wake-helper] ScheduleWake: uid" << uid
               << "-> REJECTED (rtcwake failed):" << err;
    return false;
}

bool WakeHelper::CancelWake(QString &message)
{
    const uint uid = callerUid();

    // Privilege gate — cancelling a root-scheduled wake is privileged too.
    QString denyReason;
    if (!authorize(uid, &denyReason)) {
        message = denyReason;
        qWarning() << "[wake-helper] CancelWake: uid" << uid << "-> REJECTED:" << message;
        return false;
    }

    // Enumerate from systemd first (STG-1.4): after a helper restart the
    // armed timer is not in this process's memory, and answering "idle" here
    // would be a false success while the wake is still live. Stop everything
    // found, not just the unit this instance remembers.
    const QStringList bases = armedTimerBasesFromSystemd();
    if (!bases.isEmpty()) {
        for (const QString &base : bases) {
            QString stopErr;
            if (!stopTimer(base, &stopErr)) {
                // Truthful failure: the timer is (still) armed.
                m_state = QStringLiteral("error");
                m_lastError = stopErr;
                message = stopErr;
                qWarning() << "[wake-helper] CancelWake: uid" << uid
                           << "-> REJECTED (stop failed for" << base << "):" << stopErr;
                return false;
            }
        }
        if (!armedTimerBasesFromSystemd().isEmpty()) {
            m_state = QStringLiteral("error");
            m_lastError = QStringLiteral("timer still armed after stop");
            message = m_lastError;
            qWarning() << "[wake-helper] CancelWake: uid" << uid << "-> REJECTED:" << message;
            return false;
        }
        m_unitName.clear();
        m_usedRtcwake = false;
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (systemd timer"
                << (bases.size() > 1 ? QStringLiteral("s") : QString()) << "stopped)";
        return true;
    }

    // Path B: rtcwake was used for this schedule.
    if (m_usedRtcwake || m_rtcwakePid > 0) {
        QString err;
        if (!stopRtcwake(&err)) {
            m_state = QStringLiteral("error");
            m_lastError = err;
            message = err;
            qWarning() << "[wake-helper] CancelWake: uid" << uid
                       << "-> REJECTED (disable failed):" << err;
            return false;
        }
        m_usedRtcwake = false;
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        m_armedAtMs = 0;
        removeWakeRecord(); // the wake is gone; so is the record of it
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (rtcwake stopped)";
        return true;
    }

    // Path C (STG-1.4, option 1a): this instance lost its memory of the wake
    // — it restarted after arming — but the record is still on disk. Trusting
    // memory alone here would answer "cancelled" while the alarm stayed live.
    //
    // `rtcwake -m disable` is safe to run even when nothing is armed (it
    // exits 0 and clears an unset alarm), so the record is enough to act on;
    // there is no reason to guess at a state the hardware would only confirm.
    if (wakeRecordExists()) {
        QString err;
        if (!runCommand(QStringLiteral("rtcwake"),
                        QStringList() << QStringLiteral("-m") << QStringLiteral("disable"), &err)) {
            m_state = QStringLiteral("error");
            m_lastError = err;
            message = err;
            qWarning() << "[wake-helper] CancelWake: uid" << uid
                       << "-> REJECTED (post-restart disable failed):" << err;
            return false;
        }
        m_usedRtcwake = false;
        m_rtcwakePid = 0;
        m_unitName.clear();
        m_state = QStringLiteral("idle");
        m_lastError.clear();
        m_seconds = 0;
        m_armedAtMs = 0;
        removeWakeRecord();
        message = QStringLiteral("cancelled");
        qInfo() << "[wake-helper] CancelWake: uid" << uid
                << "-> accepted (wake rediscovered from its record after restart)";
        return true;
    }

    // Nothing armed anywhere — idempotent success, and any leftover error
    // state from an earlier failed call is cleared so GetStatus() reports
    // "idle" after a successful cancel.
    m_unitName.clear();
    m_state = QStringLiteral("idle");
    m_lastError.clear();
    m_seconds = 0;
    message = QStringLiteral("idle");
    qInfo() << "[wake-helper] CancelWake: uid" << uid << "-> accepted (idle, nothing to cancel)";
    return true;
}

QString WakeHelper::GetStatus()
{
    // STG-1.4: ask the record and the RTC, not this process, so a helper
    // restart cannot report "idle" for a wake that is still armed.
    rediscoverState();

    const uint uid = callerUid();
    if (m_state == QLatin1String("scheduled")) {
        const QString status = m_seconds > 0
                                   ? QStringLiteral("scheduled:") + QString::number(m_seconds)
                                   : QStringLiteral("scheduled");
        qInfo() << "[wake-helper] GetStatus: uid" << uid << "->" << status;
        return status;
    }

    // STG-1.7/F7: GetStatus stays public and read-only — it performs no
    // privileged operation, and the fact that a Lumalarm wake is armed is
    // already world-readable from `systemctl list-timers 'lumalarm-wake-*.timer'`,
    // so gating it on polkit would buy nothing while breaking unauthenticated
    // diagnostics. What is NOT public is the free-form error detail: only the
    // state word crosses the bus to an unauthenticated caller, and the full
    // text stays in the root-only journal.
    const QString status =
        m_state == QLatin1String("error") ? QStringLiteral("error") : QStringLiteral("idle");
    qInfo() << "[wake-helper] GetStatus: uid" << uid << "->" << status;
    return status;
}

QVariantMap WakeHelper::GetCapabilities()
{
    const uint uid = callerUid();
    const QVariantMap caps =
        WakePolicy::capabilitiesToVariantMap(WakePolicy::readCapabilities(QStringLiteral("/sys")));
    qInfo() << "[wake-helper] GetCapabilities: uid" << uid << "-> sleepStates"
            << caps.value(QStringLiteral("sleepStates")).toStringList() << "memSleepDefault"
            << caps.value(QStringLiteral("memSleepDefault")).toString() << "wakeCapableRtc"
            << caps.value(QStringLiteral("wakeCapableRtc")).toString();
    return caps;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    QDBusConnection bus = QDBusConnection::systemBus();
    if (!bus.isConnected()) {
        qCritical() << "[wake-helper] cannot connect to the system bus:"
                    << bus.lastError().message();
        return 1;
    }

    WakeHelper helper;
    if (!bus.registerService(QString::fromLatin1(WakePolicy::kHelperService))) {
        qCritical() << "[wake-helper] failed to register" << WakePolicy::kHelperService << ':'
                    << bus.lastError().message()
                    << "(is the send/own policy installed in /usr/share/dbus-1/system.d/?)";
        return 1;
    }
    bus.registerObject(QString::fromLatin1(WakePolicy::kHelperPath), &helper,
                       QDBusConnection::ExportAllSlots);

    qInfo() << "[wake-helper] org.lumalarm.Helper ready on the system bus";
    return app.exec();
}

#include "lumalarm-wake-helper.moc"
